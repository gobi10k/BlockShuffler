#include "ExportRenderer.h"
#include "EntryMixer.h"
#include "SoftLimiter.h"
#include "TempoStretcher.h"
#include "../Model/Clip.h"
#include <unordered_map>
#include <limits>

namespace BlockShuffler {

bool ExportRenderer::renderToFile(const ResolvedArrangement& arrangement,
                                   const juce::File& outputFile,
                                   juce::AudioFormat& format,
                                   int bitDepth,
                                   ProgressFn progress) {
    if (arrangement.isEmpty()) return false;

    const int64_t totalSamples = arrangement.totalDurationSamples;
    if (totalSamples <= 0) return false;

    // FIX H4: guard against int overflow for very long arrangements
    if (totalSamples > (int64_t)std::numeric_limits<int>::max()) return false;

    const int     numChannels = 2;
    const int64_t numSamples  = totalSamples;  // int64_t; cast to int only at point of use

    // Allocate output buffer
    juce::AudioBuffer<float> output(numChannels, (int)numSamples);
    output.clear();

    // FIX H1: use shared mixEntryToBuffer so export and playback are identical.
    // Export always runs at project sample rate (pToH = hToP = 1.0, currentHead = 0).
    const int numEntries = arrangement.entries.size();
    for (int i = 0; i < numEntries; ++i) {
        mixEntryToBuffer(arrangement.entries.getReference(i), output, (int)numSamples, 0LL, 1.0, 1.0, i);
        if (progress) progress((float)(i + 1) / (float)numEntries);
    }

    // FIX 1(i): stateless soft-limit the FINAL summed mix so no sample exceeds
    // full scale at the writer. Same shared curve as PlaybackEngine — stateless,
    // so export stays bit-identical to block-by-block playback.
    SoftLimiter::process(output, (int)numSamples);

    // Write to file
    outputFile.deleteFile();
    auto outStream = outputFile.createOutputStream();
    if (!outStream) return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(outStream.release(),
                               arrangement.sampleRate,
                               (unsigned)numChannels,
                               bitDepth,
                               {},
                               0));
    if (!writer) return false;

    return writer->writeFromAudioSampleBuffer(output, 0, (int)numSamples);
}


bool ExportRenderer::writeClipFlac(const Clip& clip, const juce::File& dest, int bitDepth, double sampleRate) {
    if (!clip.audioBuffer) return false;
    const auto& buf = *(clip.audioBuffer);
    const int64_t bodyLen = clip.endMark - clip.startMark;
    if (bodyLen <= 0 || buf.getNumSamples() == 0) return false;

    const int numCh  = juce::jmax(1, buf.getNumChannels());
    const int srcLen = buf.getNumSamples();
    const int start  = (int)juce::jlimit((int64_t)0, (int64_t)(srcLen - 1), clip.startMark);
    const int count  = (int)juce::jmin(bodyLen, (int64_t)(srcLen - start));
    if (count <= 0) return false;

    dest.deleteFile();
    auto outStream = dest.createOutputStream();
    if (!outStream) return false;

    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        flac.createWriterFor(outStream.release(),
                             sampleRate,
                             (unsigned)numCh,
                             bitDepth, {}, 0));
    if (!writer) return false;
    return writer->writeFromAudioSampleBuffer(buf, start, count);
}

bool ExportRenderer::renderToBsf(const ResolvedArrangement& arrangement,
                                  const juce::File& outputFile,
                                  int bitDepth,
                                  ProgressFn progress,
                                  const juce::var& projectSnapshot) {
    if (arrangement.isEmpty()) return false;

    // ── 1. Collect unique clips ───────────────────────────────────────────────
    // Track unique clips by ID and store their audio buffers for export
    struct UniqueClipEntry { juce::String id; std::shared_ptr<juce::AudioBuffer<float>> buffer; int64_t start, end; };
    juce::Array<UniqueClipEntry> uniqueClips;

    for (const auto& entry : arrangement.entries) {
        bool alreadyFound = false;
        for (const auto& uc : uniqueClips) {
            if (uc.id == entry.clipId) { alreadyFound = true; break; }
        }
        if (!alreadyFound)
            uniqueClips.add({entry.clipId, entry.audioBuffer, entry.startMark, entry.endMark});
    }

    // ── 2. Write per-clip FLACs to a temp directory ───────────────────────────
    auto tmpDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                      .getChildFile("bsf_export_" + juce::String(juce::Time::currentTimeMillis()));
    tmpDir.createDirectory();
    auto clipsDir = tmpDir.getChildFile("clips");
    clipsDir.createDirectory();

    // clipId → {filename, durationSamples}
    struct ClipMeta { juce::String id; juce::String filename; int64_t durationSamples; };
    juce::Array<ClipMeta> clipMeta;
    clipMeta.resize(uniqueClips.size());

    for (int i = 0; i < uniqueClips.size(); ++i) {
        const auto& uc = uniqueClips[i];
        juce::String clipId = "clip_" + juce::String(i + 1).paddedLeft('0', 3);
        juce::String filename = "clips/" + clipId + ".flac";
        auto dest = tmpDir.getChildFile(filename);

        const int numCh  = uc.buffer ? juce::jmax(1, uc.buffer->getNumChannels()) : 1;
        const int srcLen = uc.buffer ? uc.buffer->getNumSamples() : 0;
        const int start  = (int)juce::jlimit((int64_t)0, (int64_t)juce::jmax(0, srcLen - 1), uc.start);
        const int count  = (int)juce::jmin(uc.end - uc.start, (int64_t)juce::jmax(0, srcLen - start));

        bool writeOk = false;
        if (uc.buffer && count > 0) {
            dest.deleteFile();
            auto outStream = dest.createOutputStream();
            if (outStream) {
                juce::FlacAudioFormat flac;
                std::unique_ptr<juce::AudioFormatWriter> writer(
                    flac.createWriterFor(outStream.release(), arrangement.sampleRate, (unsigned)numCh, bitDepth, {}, 0));
                if (writer) writeOk = writer->writeFromAudioSampleBuffer(*(uc.buffer), start, count);
            }
        }

        if (!writeOk) {
            tmpDir.deleteRecursively();
            return false;
        }

        clipMeta.set(i, { clipId, filename, uc.end - uc.start });

        if (progress)
            progress(0.5f * (float)(i + 1) / (float)uniqueClips.size());
    }

    // ── 3. Build manifest.json ────────────────────────────────────────────────
    auto* manifest = new juce::DynamicObject();
    manifest->setProperty("version",              1);
    manifest->setProperty("title",                juce::String("Arrangement"));
    manifest->setProperty("sampleRate",           arrangement.sampleRate);
    manifest->setProperty("bitDepth",             bitDepth);
    manifest->setProperty("totalDurationSamples", juce::String(arrangement.totalDurationSamples));

    juce::Array<juce::var> clipsArray;
    for (int i = 0; i < uniqueClips.size(); ++i) {
        auto* c = new juce::DynamicObject();
        c->setProperty("id",              clipMeta[i].id);
        c->setProperty("file",            clipMeta[i].filename);
        c->setProperty("startSample",     0);
        c->setProperty("durationSamples", juce::String(clipMeta[i].durationSamples));
        clipsArray.add(juce::var(c));
    }
    manifest->setProperty("clips", clipsArray);

    juce::Array<juce::var> arrangementArray;
    for (const auto& entry : arrangement.entries) {
        int idx = -1;
        for (int k = 0; k < uniqueClips.size(); ++k) {
            if (uniqueClips[k].id == entry.clipId) { idx = k; break; }
        }
        if (idx < 0) continue;

        auto* a = new juce::DynamicObject();
        a->setProperty("clipId",    clipMeta[idx].id);
        a->setProperty("startTime", juce::String(entry.timelinePos));
        arrangementArray.add(juce::var(a));
    }
    manifest->setProperty("arrangement", arrangementArray);

    auto manifestJson = juce::JSON::toString(juce::var(manifest), true);
    auto manifestFile = tmpDir.getChildFile("manifest.json");
    manifestFile.replaceWithText(manifestJson);

    // ── 3b. Build model.json (if project snapshot provided) ──────────────────
    // model.json contains the full probabilistic model so a mobile player can
    // re-randomise on-device. It is additive — manifest.json is unchanged.
    juce::File modelFile;
    if (projectSnapshot.isObject())
    {
        // Map Clip UUID → embedded BSF filename for clips in this resolution
        std::unordered_map<std::string, std::string> clipUuidToEmbedded;
        for (int i = 0; i < uniqueClips.size(); ++i)
            clipUuidToEmbedded[uniqueClips[i].id.toStdString()]
                = clipMeta[i].filename.toStdString();

        auto* model = new juce::DynamicObject();
        model->setProperty("version",    1);
        model->setProperty("name",       projectSnapshot.getProperty("name",       "Arrangement").toString());
        model->setProperty("sampleRate", projectSnapshot.getProperty("sampleRate", 48000.0));

        // Blocks
        juce::Array<juce::var> modelBlocks;
        if (auto* bArr = projectSnapshot.getProperty("blocks", {}).getArray())
        {
            for (const auto& bVar : *bArr)
            {
                auto* bm = new juce::DynamicObject();
                bm->setProperty("id",               bVar.getProperty("id",            ""));
                bm->setProperty("name",             bVar.getProperty("name",          ""));
                bm->setProperty("color",            bVar.getProperty("color",         ""));
                bm->setProperty("position",         bVar.getProperty("position",       0));
                bm->setProperty("stackGroup",       bVar.getProperty("stackGroup",    -1));
                bm->setProperty("stackPlayCount",   bVar.getProperty("stackPlayCount",{}));
                bm->setProperty("stackPlayMode",    bVar.getProperty("stackPlayMode", "sequential"));
                bm->setProperty("alwaysPlayBase",   bVar.getProperty("alwaysPlayBase", false));
                // isDone is cosmetic only — not written to export manifest

                // Clips
                juce::Array<juce::var> modelClips;
                if (auto* cArr = bVar.getProperty("clips", {}).getArray())
                {
                    for (const auto& cVar : *cArr)
                    {
                        auto* cm = new juce::DynamicObject();
                        cm->setProperty("id",                cVar.getProperty("id",               ""));
                        cm->setProperty("name",              cVar.getProperty("name",             ""));
                        cm->setProperty("probability",       cVar.getProperty("probability",      1.0));
                        cm->setProperty("tempo",             cVar.getProperty("tempo",            120.0));
                        cm->setProperty("retainLeadInTempo", cVar.getProperty("retainLeadInTempo",false));
                        cm->setProperty("retainTailTempo",   cVar.getProperty("retainTailTempo",  false));
                        cm->setProperty("isSongEnder",       cVar.getProperty("isSongEnder",      false));
                        // isDone is cosmetic only — not written to export manifest
                        cm->setProperty("startMark",         cVar.getProperty("startMark",        "0"));
                        cm->setProperty("endMark",           cVar.getProperty("endMark",          "0"));

                        // Reference embedded FLAC if this clip appeared in the arrangement
                        juce::String uuid = cVar.getProperty("id", "").toString();
                        auto it = clipUuidToEmbedded.find(uuid.toStdString());
                        if (it != clipUuidToEmbedded.end())
                            cm->setProperty("embeddedFile", juce::String(it->second));

                        modelClips.add(juce::var(cm));
                    }
                }
                bm->setProperty("clips", modelClips);
                modelBlocks.add(juce::var(bm));
            }
        }
        model->setProperty("blocks", modelBlocks);

        // Links
        juce::Array<juce::var> modelLinks;
        if (auto* lArr = projectSnapshot.getProperty("links", {}).getArray())
        {
            for (const auto& lVar : *lArr)
            {
                auto* lm = new juce::DynamicObject();
                lm->setProperty("blockA",          lVar.getProperty("blockA",          ""));
                lm->setProperty("blockB",          lVar.getProperty("blockB",          ""));
                lm->setProperty("swapProbability", lVar.getProperty("swapProbability", 0.5));
                modelLinks.add(juce::var(lm));
            }
        }
        model->setProperty("links", modelLinks);

        auto modelJson = juce::JSON::toString(juce::var(model), true);
        modelFile = tmpDir.getChildFile("model.json");
        modelFile.replaceWithText(modelJson);
    }

    // ── 4. ZIP everything into .bsf ───────────────────────────────────────────
    juce::ZipFile::Builder builder;
    builder.addFile(manifestFile, 6, "manifest.json");
    if (modelFile != juce::File{})
        builder.addFile(modelFile, 6, "model.json");
    for (int i = 0; i < uniqueClips.size(); ++i) {
        auto clipFile = tmpDir.getChildFile(clipMeta[i].filename);
        builder.addFile(clipFile, 0, clipMeta[i].filename);  // 0 = store (FLAC already compressed)
    }

    outputFile.deleteFile();
    auto outStream = outputFile.createOutputStream();
    bool ok = outStream && builder.writeToStream(*outStream, nullptr);

    if (progress) progress(1.0f);

    // ── 5. Clean up temp directory ────────────────────────────────────────────
    tmpDir.deleteRecursively();

    return ok;
}

} // namespace BlockShuffler
