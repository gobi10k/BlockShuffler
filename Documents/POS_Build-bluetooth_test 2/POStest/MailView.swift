//
//  MailView.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import SwiftUI
import MessageUI

struct MailView: UIViewControllerRepresentable {
    @Environment(\.presentationMode) var presentation
    @Binding var result: Result<MFMailComposeResult, Error>?

    var subject: String
    var recipients: [String]
    var messageBody: String

    // Attachment properties
    var attachmentData: Data?
    var mimeType: String?
    var fileName: String?

    class Coordinator: NSObject, MFMailComposeViewControllerDelegate {
        @Binding var presentation: PresentationMode
        @Binding var result: Result<MFMailComposeResult, Error>?

        init(presentation: Binding<PresentationMode>, result: Binding<Result<MFMailComposeResult, Error>?>) {
            _presentation = presentation
            _result = result
        }

        func mailComposeController(_ controller: MFMailComposeViewController, didFinishWith result: MFMailComposeResult, error: Error?) {
            defer {
                $presentation.wrappedValue.dismiss()
            }
            guard error == nil else {
                self.result = .failure(error!)
                return
            }
            self.result = .success(result)
        }
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(presentation: presentation, result: $result)
    }

    func makeUIViewController(context: UIViewControllerRepresentableContext<MailView>) -> MFMailComposeViewController {
        let vc = MFMailComposeViewController()
        vc.mailComposeDelegate = context.coordinator
        vc.setSubject(subject)
        vc.setToRecipients(recipients)
        vc.setMessageBody(messageBody, isHTML: false)

        // Add attachment if available
        if let attachmentData = attachmentData, let mimeType = mimeType, let fileName = fileName {
            vc.addAttachmentData(attachmentData, mimeType: mimeType, fileName: fileName)
        }

        return vc
    }

    func updateUIViewController(_ uiViewController: MFMailComposeViewController, context: UIViewControllerRepresentableContext<MailView>) {}
}
