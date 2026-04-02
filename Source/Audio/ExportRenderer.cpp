#include "ExportRenderer.h"
#include "TempoStretcher.h"
#include "../Model/Clip.h"
#include <unordered_map>

namespace BlockShuffler {

bool ExportRenderer::renderToFile(const ResolvedArrangement& arrangement,
                                   const juce::File& outputFile,
                                   juce::AudioFormat& format,
                                   int bitDepth,
                                   ProgressFn progress) {
    if (arrangement.isEmpty()) return false;

    const int64_t totalSamples = arrangement.totalDurationSamples;
    if (totalSamples <= 0) return false;

    // Warn if arrangement is too long for a single AudioBuffer (~12h at 48kHz)
    if (totalSamples > (int64_t)std::numeric_limits<int>::max())
        DBG("Warning: Arrangement exceeds INT_MAX samples. Truncating to " + juce::String(std::numeric_limits<int>::max()) + " samples.");

    const int numChannels = 2;
    const int numSamples  = (int)juce::jmin(totalSamples, (int64_t)std::numeric_limits<int>::max());

    // Allocate output buffer
    juce::AudioBuffer<float> output(numChannels, numSamples);
    output.clear();

    // Mix every entry
    const int numEntries = arrangement.entries.size();
    for (int i = 0; i < numEntries; ++i) {
        mixEntry(output, arrangement.entries.getReference(i));
        if (progress) progress((float)(i + 1) / (float)numEntries);
    }

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

    DBG("Export: writer sampleRate=" + juce::String(arrangement.sampleRate)
        + " totalDurationSamples=" + juce::String(arrangement.totalDurationSamples)
        + " numEntries=" + juce::String(arrangement.entries.size()));

    return writer->writeFromAudioSampleBuffer(output, 0, numSamples);
}

void ExportRenderer::mixEntry(juce::AudioBuffer<float>& dest,
                               const ResolvedEntry& entry)
{
    const auto& src  = entry.clip->audioBuffer;
    const int srcCh  = src.getNumChannels();
    const int srcLen = src.getNumSamples();
    const int dstCh  = dest.getNumChannels();
    const int dstLen = dest.getNumSamples();
    if (srcCh == 0 || srcLen == 0 || dstCh == 0) return;

    const int64_t startMark = entry.clip->startMark;
    const int64_t endMark   = entry.clip->endMark;
    const int64_t bodyLen   = endMark - startMark;
    const int64_t leadInLen = startMark;
    const int64_t tailLen   = juce::jmax((int64_t)0, (int64_t)srcLen - endMark);
    const int64_t bodyStart = entry.timelinePos;
    const int64_t bodyEnd   = bodyStart + bodyLen;

    // General 1:1 additive mixer with gain ramp, from any source buffer.
    auto mixBufRange = [&](const juce::AudioBuffer<float>& s,
                            int64_t tlStart, int64_t tlEnd,
                            int64_t srcOff0,
                            float gainStart, float gainEnd)
    {
        const int sCh  = s.getNumChannels();
        const int sLen = s.getNumSamples();
        const int mCh  = juce::jmin(sCh, dstCh);
        if (sCh == 0 || sLen == 0) return;

        int64_t ovStart = juce::jmax(tlStart, (int64_t)0);
        int64_t ovEnd   = juce::jmin(tlEnd,   (int64_t)dstLen);
        if (ovStart >= ovEnd) return;

        int   destOff = (int)ovStart;
        int64_t srcOff = srcOff0 + (ovStart - tlStart);
        int   count   = (int)(ovEnd - ovStart);
        if (srcOff < 0 || srcOff >= (int64_t)sLen) return;
        count = juce::jmin(count, sLen - (int)srcOff);
        if (count <= 0) return;

        int64_t regLen = tlEnd - tlStart;
        float gs = gainStart, ge = gainEnd;
        if (regLen > 1) {
            float t0 = (float)(ovStart - tlStart) / (float)regLen;
            float t1 = (float)(ovEnd   - tlStart) / (float)regLen;
            gs = gainStart + t0 * (gainEnd - gainStart);
            ge = gainStart + t1 * (gainEnd - gainStart);
        }
        gs *= entry.gain;
        ge *= entry.gain;

        for (int ch = 0; ch < mCh; ++ch)
            dest.addFromWithRamp(ch, destOff, s.getReadPointer(ch) + srcOff, count, gs, ge);
        if (sCh == 1 && dstCh >= 2)
            dest.addFromWithRamp(1, destOff, s.getReadPointer(0) + srcOff, count, gs, ge);
    };

    // ── Lead-in ────────────────────────────────────────────────────────────────
    if (leadInLen > 0)
    {
        if (entry.stretchedLeadIn)
        {
            int64_t sl = (int64_t)entry.stretchedLeadIn->getNumSamples();
            mixBufRange(*entry.stretchedLeadIn, bodyStart - sl, bodyStart, 0, 0.0f, 1.0f);
        }
        else
        {
            int64_t leadInTL = (int64_t)(leadInLen * entry.leadInStretchRatio + 0.5f);
            int64_t lStart   = bodyStart - leadInTL;
            int64_t ovStart  = juce::jmax(lStart, (int64_t)0);
            int64_t ovEnd    = juce::jmin(bodyStart, (int64_t)dstLen);
            if (ovStart < ovEnd) {
                int destOff   = (int)ovStart;
                int destCount = (int)(ovEnd - ovStart);
                if (std::abs(entry.leadInStretchRatio - 1.0f) < 0.0001f) {
                    int srcOff = juce::jmin((int)(ovStart - lStart), srcLen - 1);
                    int cnt    = juce::jmin(destCount, srcLen - srcOff);
                    float t0 = (leadInTL>1)?(float)(ovStart-lStart)/(float)(leadInTL-1):0.0f;
                    float t1 = (leadInTL>1)?(float)(ovEnd-lStart)  /(float)(leadInTL-1):1.0f;
                    for (int ch=0;ch<juce::jmin(srcCh,dstCh);++ch)
                        dest.addFromWithRamp(ch,destOff,src.getReadPointer(ch)+srcOff,cnt,t0*entry.gain,t1*entry.gain);
                    if (srcCh==1&&dstCh>=2)
                        dest.addFromWithRamp(1,destOff,src.getReadPointer(0)+srcOff,cnt,t0*entry.gain,t1*entry.gain);
                } else {
                    double srcAdv = (double)leadInLen/(double)leadInTL;
                    int srcSliceOff = (int)((double)(ovStart-lStart)*srcAdv);
                    int srcSliceCnt = juce::jmin((int)((double)destCount*srcAdv+1.5),(int)leadInLen-srcSliceOff);
                    float t0=(leadInTL>1)?(float)(ovStart-lStart)/(float)(leadInTL-1):0.0f;
                    float t1=(leadInTL>1)?(float)(ovEnd-lStart)  /(float)(leadInTL-1):1.0f;
                    TempoStretcher::resampleAdd(src,srcSliceOff,srcSliceCnt,dest,destOff,destCount,t0*entry.gain,t1*entry.gain);
                }
            }
        }
    }

    // ── Body ──────────────────────────────────────────────────────────────────
    if (bodyLen > 0)
        mixBufRange(src, bodyStart, bodyEnd, startMark, 1.0f, 1.0f);

    // ── Tail ──────────────────────────────────────────────────────────────────
    if (tailLen > 0)
    {
        // retainTailTempo=true → always use original clip audio at 1:1.
        if (entry.clip->retainTailTempo || !entry.stretchedTail)
        {
            mixBufRange(src, bodyEnd, bodyEnd + tailLen, endMark, 1.0f, 0.0f);
        }
        else
        {
            // Pre-stretched buffer (retainTailTempo=false, tempos differ)
            int64_t sl = (int64_t)entry.stretchedTail->getNumSamples();
            mixBufRange(*entry.stretchedTail, bodyEnd, bodyEnd + sl, 0, 1.0f, 0.0f);
        }
    }
}

bool ExportRenderer::writeClipFlac(const Clip& clip, const juce::File& dest, int bitDepth, double sampleRate) {
    const auto& buf = clip.audioBuffer;
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
    // Map clip pointer → stable index for naming
    juce::Array<const Clip*> uniqueClips;
    for (const auto& entry : arrangement.entries) {
        if (!uniqueClips.contains(entry.clip))
            uniqueClips.add(entry.clip);
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
        const auto* clip = uniqueClips[i];
        juce::String clipId = "clip_" + juce::String(i + 1).paddedLeft('0', 3);
        juce::String filename = "clips/" + clipId + ".flac";
        auto dest = tmpDir.getChildFile(filename);

        if (!writeClipFlac(*clip, dest, bitDepth, arrangement.sampleRate)) {
            tmpDir.deleteRecursively();
            return false;
        }

        clipMeta.set(i, { clipId, filename, clip->endMark - clip->startMark });

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
        int idx = uniqueClips.indexOf(entry.clip);
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
            clipUuidToEmbedded[uniqueClips[i]->id.toStdString()]
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
                bm->setProperty("isOverlapping",    bVar.getProperty("isOverlapping", false));
                bm->setProperty("overlapProbability", bVar.getProperty("overlapProb", 0.0));
                bm->setProperty("isDone",           bVar.getProperty("isDone",        false));

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
                        cm->setProperty("isDone",            cVar.getProperty("isDone",           false));
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
