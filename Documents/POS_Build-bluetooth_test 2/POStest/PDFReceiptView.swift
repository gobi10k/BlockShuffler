import SwiftUI

struct PDFReceiptView: View {
    let table: Table
    let posData: POSData

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            // Header
            Text("Invoice / Receipt")
                .font(.largeTitle)
                .fontWeight(.bold)
                .padding(.bottom, 8)

            HStack {
                VStack(alignment: .leading) {
                    Text("Table: \(table.id)")
                    Text("Date: \(Date(), formatter: itemFormatter)")
                }
                Spacer()
                Text("Your Business Name") // Placeholder
                    .font(.headline)
            }
            .padding(.bottom, 20)

            // Items
            Text("Items")
                .font(.title2)
                .fontWeight(.semibold)

            LazyVStack(alignment: .leading, spacing: 2) {
                ForEach(Array(table.items.enumerated()), id: \.offset) { (index, item) in
                    HStack {
                        Text(item.menuItem.name)
                        Spacer()
                        Text(String(format: "$%.2f", item.menuItem.price))
                    }
                    .padding(.vertical, 2)
                }
            }

            Divider()

            // Totals
            VStack(spacing: 8) {
                HStack {
                    Text("Subtotal")
                        .fontWeight(.semibold)
                    Spacer()
                    Text(String(format: "$%.2f", table.subtotal))
                }
                HStack {
                    Text("Tax")
                        .fontWeight(.semibold)
                    Spacer()
                    Text(String(format: "$%.2f", table.tax))
                }
                HStack {
                    Text("Total")
                        .font(.title3)
                        .fontWeight(.bold)
                    Spacer()
                    Text(String(format: "$%.2f", table.total))
                        .font(.title3)
                        .fontWeight(.bold)
                }
            }
            .padding(.top, 8)

            Spacer()

            // Footer
            Text("Thank you for your business!")
                .font(.footnote)
                .foregroundColor(.gray)
                .padding(.top, 20)
        }
        .padding(40)
        .frame(width: 595, height: 842) // A4 paper size in points
    }

    private var itemFormatter: DateFormatter {
        let formatter = DateFormatter()
        formatter.dateStyle = .long
        formatter.timeStyle = .short
        return formatter
    }
}

// Preview Provider for PDFReceiptView
struct PDFReceiptView_Previews: PreviewProvider {
    static var previews: some View {
        let table = Table(
            id: 5,
            items: [
                OrderItem(menuItem: MenuItem(name: "Craft Burger", price: 18.99, taxCategory: .nonAlcoholic, unitCost: 8.50), timestamp: Date()),
                OrderItem(menuItem: MenuItem(name: "IPA Beer", price: 7.50, taxCategory: .alcoholic, unitCost: 3.00), timestamp: Date()),
                OrderItem(menuItem: MenuItem(name: "Truffle Fries", price: 12.49, taxCategory: .nonAlcoholic, unitCost: 4.50), timestamp: Date())
            ],
            guestCount: 4,
            openedAt: Date()
        )

        PDFReceiptView(table: table, posData: POSData())
    }
}
