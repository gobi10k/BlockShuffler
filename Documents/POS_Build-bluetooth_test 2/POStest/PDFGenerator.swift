import SwiftUI
import CoreGraphics

struct PDFGenerator {
    @MainActor
    static func generate(from view: some View) -> Data {
        // Use the frame specified in the view for the PDF page size
        let renderer = ImageRenderer(content: view)

        let url = URL.documentsDirectory.appending(path: "receipt.pdf")

        renderer.render { size, context in
            var box = CGRect(x: 0, y: 0, width: size.width, height: size.height)

            guard let pdf = CGContext(url as CFURL, mediaBox: &box, nil) else {
                print("Failed to create PDF context.")
                return
            }

            pdf.beginPDFPage(nil)
            context(pdf)
            pdf.endPDFPage()
            pdf.closePDF()
        }

        do {
            let data = try Data(contentsOf: url)
            // It's good practice to clean up the temporary file.
            try? FileManager.default.removeItem(at: url)
            return data
        } catch {
            print("Failed to read PDF data from temporary file: \(error)")
            return Data()
        }
    }
}
