import SwiftUI

struct PDFReceiptView: View {
    let table: Table
    let posData: POSData
    var tipAmount: Double = 0.0

    @AppStorage("businessName") private var businessName: String = "Five Points Cocktail Bar"
    @AppStorage("location")     private var location: String     = ""
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    private var subtotal: Double { table.total - table.tax }
    private var grandTotal: Double { table.total + tipAmount }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {

            // MARK: Header
            HStack(alignment: .top) {
                VStack(alignment: .leading, spacing: 4) {
                    Text(businessName)
                        .font(.system(size: 20, weight: .bold))
                    if !location.isEmpty {
                        Text(location)
                            .font(.system(size: 11))
                            .foregroundColor(.secondary)
                    }
                }
                Spacer()
                VStack(alignment: .trailing, spacing: 4) {
                    Text("Receipt")
                        .font(.system(size: 20, weight: .bold))
                    Text("Table \(table.id)")
                        .font(.system(size: 13))
                        .foregroundColor(.secondary)
                    Text(Date(), formatter: dateFormatter)
                        .font(.system(size: 11))
                        .foregroundColor(.secondary)
                }
            }
            .padding(.bottom, 20)

            Divider()

            // MARK: Items
            VStack(alignment: .leading, spacing: 0) {
                // Column headers
                HStack {
                    Text("Item")
                        .font(.system(size: 11, weight: .semibold))
                        .foregroundColor(.secondary)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    Text("Price")
                        .font(.system(size: 11, weight: .semibold))
                        .foregroundColor(.secondary)
                        .frame(width: 70, alignment: .trailing)
                }
                .padding(.vertical, 6)

                Divider()

                ForEach(Array(table.items.enumerated()), id: \.offset) { _, item in
                    VStack(alignment: .leading, spacing: 2) {
                        HStack(alignment: .top) {
                            Text(item.menuItem.name)
                                .font(.system(size: 13, weight: .medium))
                                .frame(maxWidth: .infinity, alignment: .leading)
                            Text(item.finalPrice, format: .currency(code: currencyCode))
                                .font(.system(size: 13, weight: .medium))
                                .frame(width: 70, alignment: .trailing)
                        }

                        // Modifiers
                        ForEach(item.appliedModifiers) { modifier in
                            HStack {
                                Text("  + \(modifier.name)")
                                    .font(.system(size: 11))
                                    .foregroundColor(.secondary)
                                Spacer()
                                if modifier.priceAdjustment != 0 {
                                    Text(modifier.displayAdjustment)
                                        .font(.system(size: 11))
                                        .foregroundColor(.secondary)
                                        .frame(width: 70, alignment: .trailing)
                                }
                            }
                        }

                        // Note
                        if let note = item.note, !note.isEmpty {
                            Text("  \u{2192} \(note)")
                                .font(.system(size: 11).italic())
                                .foregroundColor(.secondary)
                        }
                    }
                    .padding(.vertical, 5)

                    Divider().opacity(0.4)
                }
            }
            .padding(.vertical, 8)

            // MARK: Totals
            VStack(spacing: 6) {
                totalRow(label: "Subtotal (excl. tax)", value: subtotal, bold: false)

                // Tax breakdown by band
                let taxBreakdown = taxBreakdown()
                ForEach(taxBreakdown, id: \.label) { row in
                    totalRow(label: row.label, value: row.amount, bold: false, secondary: true)
                }

                Divider().padding(.vertical, 2)

                totalRow(label: "Order Total", value: table.total, bold: true)

                if tipAmount > 0 {
                    totalRow(label: "Tip", value: tipAmount, bold: false)
                    Divider().padding(.vertical, 2)
                    totalRow(label: "Total (incl. tip)", value: grandTotal, bold: true, large: true)
                }
            }
            .padding(.top, 8)

            Spacer()

            // MARK: Footer
            Divider().padding(.top, 20)
            Text("Thank you for your visit — see you again soon!")
                .font(.system(size: 11))
                .foregroundColor(.secondary)
                .padding(.top, 8)
        }
        .padding(40)
        .frame(width: 595, height: 842) // A4 in points
        .background(Color.white)
    }

    // MARK: - Helpers

    private struct TaxRow { let label: String; let amount: Double }

    private func taxBreakdown() -> [TaxRow] {
        var buckets: [String: Double] = [:]
        for item in table.items {
            let bandName = item.menuItem.getTaxBandName(using: posData.taxBands)
            let rate     = item.menuItem.getTaxRate(using: posData.taxBands)
            let price    = item.finalPrice
            let tax      = price - (price / (1 + rate))
            buckets[bandName, default: 0] += tax
        }
        return buckets
            .map { TaxRow(label: "Tax (\($0.key))", amount: $0.value) }
            .sorted { $0.label < $1.label }
    }

    private func totalRow(
        label: String,
        value: Double,
        bold: Bool,
        secondary: Bool = false,
        large: Bool = false
    ) -> some View {
        HStack {
            Text(label)
                .font(.system(size: large ? 14 : 12, weight: bold ? .bold : .regular))
                .foregroundColor(secondary ? .secondary : .primary)
            Spacer()
            Text(value, format: .currency(code: currencyCode))
                .font(.system(size: large ? 14 : 12, weight: bold ? .bold : .regular))
                .foregroundColor(secondary ? .secondary : .primary)
        }
    }

    private let dateFormatter: DateFormatter = {
        let f = DateFormatter()
        f.dateStyle = .medium
        f.timeStyle = .short
        return f
    }()
}

// MARK: - Preview

#Preview {
    PDFReceiptView(
        table: Table(
            id: 5,
            items: [
                OrderItem(
                    menuItem: MenuItem(name: "Negroni", price: 11.00, taxCategory: .alcoholic, unitCost: 3.20, category: .cocktails),
                    timestamp: Date(),
                    appliedModifiers: [],
                    note: "no ice"
                ),
                OrderItem(
                    menuItem: MenuItem(name: "Draught Pils", price: 5.50, taxCategory: .alcoholic, unitCost: 1.50, category: .beer),
                    timestamp: Date()
                ),
                OrderItem(
                    menuItem: MenuItem(name: "Sparkling Water", price: 3.00, taxCategory: .nonAlcoholic, unitCost: 0.50, category: .softDrinks),
                    timestamp: Date()
                ),
            ],
            guestCount: 2,
            openedAt: Date().addingTimeInterval(-3600)
        ),
        posData: POSData(),
        tipAmount: 2.50
    )
}
