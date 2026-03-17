import SwiftUI
import MessageUI

struct ReceiptView: View {
    @EnvironmentObject var posData: POSData
    @Binding var isPresented: Bool
    let table: Table

    @AppStorage("defaultEmailBody") private var defaultEmailBody: String = "Thank you for your purchase. Please find your receipt attached."
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    @State private var tipAmountString: String = ""
    @State private var customerEmail: String = ""

    private let presetTipPercentages: [Int] = [10, 15, 20]

    // Mail properties
    @State private var showMailView = false
    @State private var mailResult: Result<MFMailComposeResult, Error>? = nil
    @State private var pdfData: Data?
    @State private var mailBody: String = ""

    // Alert properties
    @State private var showMailAlert = false
    @State private var mailAlertTitle = ""
    @State private var mailAlertMessage = ""

    // MARK: - Computed Properties
    private var tipAmount: Double {
        Double(tipAmountString) ?? 0.0
    }

    private var grandTotal: Double {
        table.total + tipAmount
    }

    private var alcoholicTax: Double {
        table.items.reduce(0) { total, item in
            if item.menuItem.taxCategory == .alcoholic {
                let price = item.finalPrice
                let taxAmount = price - (price / (1 + item.menuItem.taxCategory.rate))
                return total + taxAmount
            }
            return total
        }
    }
    
    private var nonAlcoholicTax: Double {
        table.items.reduce(0) { total, item in
            if item.menuItem.taxCategory == .nonAlcoholic {
                let price = item.finalPrice
                let taxAmount = price - (price / (1 + item.menuItem.taxCategory.rate))
                return total + taxAmount
            }
            return total
        }
    }
    
    private var subtotalBeforeTax: Double {
        table.total - alcoholicTax - nonAlcoholicTax
    }

    var body: some View {
        ScrollView {
            VStack(spacing: 24) {
                headerSection
                orderItemsSection
                paymentSummarySection
                emailSection
                paymentButtonsSection
            }
            .padding(20)
        }
        .background(POSColors.backgroundGradient)
        .navigationBarHidden(true)
        .sheet(isPresented: $showMailView, onDismiss: {
            handleMailResult(mailResult)
        }) {
            if let pdfData = self.pdfData {
                MailView(result: $mailResult,
                         subject: "Your Receipt",
                         recipients: [customerEmail],
                         messageBody: mailBody,
                         attachmentData: pdfData,
                         mimeType: "application/pdf",
                         fileName: "Receipt-\(table.id).pdf")
            } else {
                Text("Error generating PDF. Please try again.")
            }
        }
        .alert(isPresented: $showMailAlert) {
            Alert(title: Text(mailAlertTitle), message: Text(mailAlertMessage), dismissButton: .default(Text("OK")))
        }
        .preferredColorScheme(.dark)
    }
    
    // MARK: - Subviews
    private var headerSection: some View {
        VStack(spacing: 8) {
            Text("Receipt")
                .font(.largeTitle.weight(.bold))
                .foregroundColor(POSColors.textPrimary)

            Text("Table \(table.id)")
                .font(.title2.weight(.semibold))
                .foregroundColor(POSColors.goldPrimary)
        }
    }
    
    private var orderItemsSection: some View {
        VStack(spacing: 16) {
            HStack {
                Text("Order Items")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
            }
            
            VStack(spacing: 8) {
                ForEach(table.items) { item in
                    VStack(alignment: .leading, spacing: 4) {
                        HStack {
                            Text(item.menuItem.name)
                                .font(.body)
                                .foregroundColor(POSColors.textSecondary)
                            Spacer()
                            Text(item.finalPrice.formatted(.currency(code: currencyCode)))
                                .font(.body.weight(.medium))
                                .foregroundColor(POSColors.textPrimary)
                        }
                        
                        if !item.appliedModifiers.isEmpty {
                            HStack {
                                Text("+ \(item.appliedModifiers.map { $0.name }.joined(separator: ", "))")
                                    .font(.caption)
                                    .foregroundColor(POSColors.goldPrimary)
                                Spacer()
                            }
                            .padding(.leading, 16)
                        }
                    }
                    .padding(.vertical, 4)
                }
            }
        }
        .padding(20)
        .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))
    }
    
    private var paymentSummarySection: some View {
        VStack(spacing: 16) {
            HStack {
                Text("Payment Summary")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
            }
            
            VStack(spacing: 12) {
                summaryRow(label: "Subtotal (excl. tax)", value: subtotalBeforeTax.formatted(.currency(code: currencyCode)))
                
                if nonAlcoholicTax > 0 {
                    summaryRow(label: "Tax (9% - Non-Alcoholic)", value: nonAlcoholicTax.formatted(.currency(code: currencyCode)))
                }
                
                if alcoholicTax > 0 {
                    summaryRow(label: "Tax (21% - Alcohol)", value: alcoholicTax.formatted(.currency(code: currencyCode)))
                }
                
                summaryRow(label: "Order Total", value: table.total.formatted(.currency(code: currencyCode)), isHighlighted: true)
                
                Divider().background(POSColors.goldSubtle.opacity(0.3))

                VStack(spacing: 10) {
                    HStack {
                        Text("Tip")
                            .font(.headline.weight(.medium))
                            .foregroundColor(POSColors.textPrimary)
                        Spacer()
                        TextField("0.00", text: $tipAmountString)
                            .font(.headline.weight(.medium))
                            .foregroundColor(POSColors.goldPrimary)
                            .multilineTextAlignment(.trailing)
                            .keyboardType(.decimalPad)
                            .frame(width: 100)
                    }

                    // Preset tip buttons
                    HStack(spacing: 8) {
                        ForEach(presetTipPercentages, id: \.self) { pct in
                            let amount = (table.total * Double(pct) / 100.0)
                            Button {
                                tipAmountString = String(format: "%.2f", amount)
                            } label: {
                                VStack(spacing: 2) {
                                    Text("\(pct)%")
                                        .font(.subheadline.weight(.bold))
                                    Text(amount, format: .currency(code: currencyCode))
                                        .font(.caption2)
                                }
                                .foregroundColor(
                                    tipAmountString == String(format: "%.2f", amount)
                                        ? POSColors.backgroundDark
                                        : POSColors.goldPrimary
                                )
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 10)
                                .background(
                                    RoundedRectangle(cornerRadius: POSRadius.small)
                                        .fill(
                                            tipAmountString == String(format: "%.2f", amount)
                                                ? POSColors.goldPrimary
                                                : POSColors.goldPrimary.opacity(0.12)
                                        )
                                )
                            }
                        }

                        Button {
                            tipAmountString = "0.00"
                        } label: {
                            Text("No tip")
                                .font(.subheadline.weight(.semibold))
                                .foregroundColor(
                                    tipAmountString == "0.00" || tipAmountString == "0" || tipAmountString.isEmpty
                                        ? POSColors.backgroundDark
                                        : POSColors.textMuted
                                )
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 10)
                                .background(
                                    RoundedRectangle(cornerRadius: POSRadius.small)
                                        .fill(
                                            (tipAmountString == "0.00" || tipAmountString == "0" || tipAmountString.isEmpty)
                                                ? POSColors.textMuted
                                                : POSColors.backgroundLight.opacity(0.3)
                                        )
                                )
                        }
                    }
                }
                
                Divider().background(POSColors.goldPrimary.opacity(0.5))
                
                HStack {
                    Text("Total with Tip")
                        .font(.title2.weight(.bold))
                        .foregroundColor(POSColors.textPrimary)
                    Spacer()
                    Text(grandTotal.formatted(.currency(code: currencyCode)))
                        .font(.title2.weight(.bold))
                        .foregroundColor(POSColors.goldPrimary)
                }
            }
        }
        .padding(20)
        .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))
    }
    
    private var emailSection: some View {
        VStack(spacing: 16) {
            HStack {
                Text("Send Receipt")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
            }
            
            HStack(spacing: 12) {
                TextField("customer@example.com", text: $customerEmail)
                    .font(.body)
                    .foregroundColor(POSColors.textPrimary)
                    .keyboardType(.emailAddress)
                    .autocapitalization(.none)
                    .padding(12)
                    .background(RoundedRectangle(cornerRadius: POSRadius.small).fill(POSColors.backgroundLight.opacity(0.3)))

                Button(action: { sendReceipt() }) {
                    Image(systemName: "paperplane.fill")
                        .font(.title3.weight(.semibold))
                        .foregroundColor(POSColors.backgroundDark)
                        .frame(width: 48, height: 48)
                        .background(
                            RoundedRectangle(cornerRadius: POSRadius.small)
                                .fill(POSColors.goldGradient)
                        )
                }
                .disabled(customerEmail.isEmpty || !isValidEmail(customerEmail))
            }
        }
        .padding(20)
        .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))
    }
    
    private var paymentButtonsSection: some View {
        VStack(spacing: 16) {
            Button("Pay with Cash") { completePayment(method: "Cash") }
                .buttonStyle(PrimaryButtonStyle())

            Button("Pay with SumUp") {
                SumUpAPIManager.shared.processPayment(amount: grandTotal, currency: currencyCode) { success in
                    if success { completePayment(method: "SumUp") }
                }
            }
            .buttonStyle(PrimaryButtonStyle())
            
            Button("Cancel") { isPresented = false }
                .buttonStyle(SecondaryButtonStyle())
        }
    }
    
    // MARK: - Helper Methods
    @MainActor
    private func sendReceipt() {
        guard isValidEmail(customerEmail) else { return }

        let pdfView = PDFReceiptView(table: table, posData: posData)
        let data = PDFGenerator.generate(from: pdfView)

        guard !data.isEmpty else {
            mailAlertTitle = "Error"
            mailAlertMessage = "Could not generate PDF receipt."
            showMailAlert = true
            return
        }

        self.pdfData = data
        self.mailBody = defaultEmailBody

        if MFMailComposeViewController.canSendMail() {
            self.showMailView = true
        } else {
            mailAlertTitle = "Cannot Send Mail"
            mailAlertMessage = "This device is not configured to send email."
            showMailAlert = true
        }
    }

    private func handleMailResult(_ result: Result<MFMailComposeResult, Error>?) {
        guard let result = result else { return }
        switch result {
        case .success(let mailResult):
            switch mailResult {
            case .sent:
                mailAlertTitle = "Email Sent"
                mailAlertMessage = "The receipt has been sent successfully."
            case .saved:
                mailAlertTitle = "Email Saved"
                mailAlertMessage = "Your email has been saved as a draft."
            case .cancelled:
                mailAlertTitle = "Email Cancelled"
                mailAlertMessage = "The email has been cancelled."
            case .failed:
                mailAlertTitle = "Email Failed"
                mailAlertMessage = "Failed to send the email. Please try again."
            @unknown default:
                mailAlertTitle = "Unknown Error"
                mailAlertMessage = "An unknown error occurred."
            }
        case .failure(let error):
            mailAlertTitle = "Email Error"
            mailAlertMessage = error.localizedDescription
        }
        showMailAlert = true
    }
    
    private func isValidEmail(_ email: String) -> Bool {
        let emailRegex = "[A-Z0-9a-z._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,64}"
        let emailPredicate = NSPredicate(format:"SELF MATCHES %@", emailRegex)
        return emailPredicate.evaluate(with: email)
    }
    
    private func completePayment(method: String) {
        posData.closeTable(
            id: table.id,
            tip: tipAmount,
            customerEmail: customerEmail.isEmpty ? nil : customerEmail,
            paymentMethod: method
        )
        isPresented = false
    }

    private func summaryRow(label: String, value: String, isHighlighted: Bool = false) -> some View {
        HStack {
            Text(label)
                .font(isHighlighted ? .headline.weight(.medium) : .body.weight(.medium))
                .foregroundColor(isHighlighted ? POSColors.textPrimary : POSColors.textSecondary)
            Spacer()
            Text(value)
                .font(isHighlighted ? .headline.weight(.semibold) : .body.weight(.medium))
                .foregroundColor(isHighlighted ? POSColors.goldPrimary : POSColors.textSecondary)
        }
    }
}

struct ReceiptView_Previews: PreviewProvider {
    static var previews: some View {
        let table = Table(id: 5, items: [], guestCount: 4, openedAt: Date())
        ReceiptView(isPresented: .constant(true), table: table)
            .environmentObject(POSData())
            .preferredColorScheme(.dark)
    }
}
