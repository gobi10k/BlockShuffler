//
//  DailyReportView.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import SwiftUI

struct DailyReportView: View {
    @EnvironmentObject var posData: POSData
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    private var closedTables: [ClosedTable] {
        posData.currentDay?.closedTables ?? []
    }

    private var dailyTotal: Double {
        closedTables.reduce(0) { $0 + $1.total }
    }

    private var dailyTax: Double {
        closedTables.reduce(0) { $0 + $1.taxAmount }
    }

    private var dailyTips: Double {
        closedTables.reduce(0) { $0 + $1.tipAmount }
    }

    private var itemsPerGuest: Double {
        let totalItems = closedTables.reduce(0) { $0 + $1.items.count }
        let totalGuests = closedTables.reduce(0) { $0 + $1.guestCount }
        guard totalGuests > 0 else { return 0 }
        return Double(totalItems) / Double(totalGuests)
    }

    private var itemsPerHour: Double {
        guard let day = posData.currentDay else { return 0 }
        let totalItems = closedTables.reduce(0) { $0 + $1.items.count }
        let duration = day.durationInHours
        guard duration > 0 else { return 0 }
        return Double(totalItems) / duration
    }

    private var taxBreakdown: [TaxBand: Double] {
        var breakdown: [TaxBand: Double] = [:]

        for table in closedTables {
            for item in table.items {
                let taxRate = item.menuItem.getTaxRate(using: posData.taxBands)
                if taxRate > 0 {
                    let taxAmount = item.finalPrice - (item.finalPrice / (1 + taxRate))

                    if let taxBand = posData.taxBands.first(where: { $0.rate == taxRate }) {
                         breakdown[taxBand, default: 0] += taxAmount
                    } else {
                        // Fallback for older items that might use TaxCategory
                        let fallbackBand = TaxBand(name: item.menuItem.taxCategory.rawValue, rate: item.menuItem.taxCategory.rate)
                        breakdown[fallbackBand, default: 0] += taxAmount
                    }
                }
            }
        }
        return breakdown
    }

    private var staffCost: Double {
        guard let day = posData.currentDay else { return 0 }
        let duration = day.durationInHours

        let totalRate = day.staff.reduce(0.0) { total, employeeName in
            let employeeRate = posData.employees.first { $0.name == employeeName }?.hourlyRate ?? 0.0
            return total + employeeRate
        }

        return totalRate * duration
    }

    var body: some View {
        NavigationStack {
            VStack {
                if #available(iOS 16.0, *) {
                    SwiftUI.Table(closedTables) {
                        TableColumn("Table ID") { table in Text("\(table.tableID)") }
                        TableColumn("Guests") { table in Text("\(table.guestCount)") }
                        TableColumn("Efficiency") { table in
                            if table.maxGuests > 0 {
                                Text(Double(table.guestCount) / Double(table.maxGuests), format: .percent)
                            } else {
                                Text("N/A")
                            }
                        }
                        TableColumn("Duration") { table in
                            Text(formatDuration(from: table.openedAt, to: table.closedAt))
                        }
                        TableColumn("Tax") { table in
                            Text(table.taxAmount, format: .currency(code: currencyCode))
                        }
                        TableColumn("Tip") { table in
                            Text(table.tipAmount, format: .currency(code: currencyCode))
                        }
                        TableColumn("Total") { table in
                            Text(table.total, format: .currency(code: currencyCode))
                        }
                    }
                } else {
                    // Fallback for earlier iOS versions
                    List(closedTables) { table in
                        VStack(alignment: .leading) {
                            Text("Table: \(table.tableID)")
                                .font(.headline)
                            HStack {
                                Text("Guests: \(table.guestCount)")
                                Spacer()
                                if table.maxGuests > 0 {
                                    Text("Efficiency: \(Double(table.guestCount) / Double(table.maxGuests), format: .percent)")
                                }
                                Spacer()
                                Text("Total: \(table.total, format: .currency(code: currencyCode))")
                            }
                            Text("Duration: \(formatDuration(from: table.openedAt, to: table.closedAt))")
                                .font(.subheadline)
                                .foregroundColor(.secondary)
                        }
                    }
                }

                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("Daily Total:")
                            .font(.headline)
                        Spacer()
                        Text(dailyTotal, format: .currency(code: currencyCode))
                            .font(.headline)
                    }

                    ForEach(taxBreakdown.sorted(by: { $0.key.rate < $1.key.rate }), id: \.key.id) { band, amount in
                        HStack {
                            Text("Tax (\(band.name)):")
                                .font(.headline)
                            Spacer()
                            Text(amount, format: .currency(code: currencyCode))
                                .font(.headline)
                        }
                    }

                    HStack {
                        Text("Daily Tips:")
                            .font(.headline)
                        Spacer()
                        Text(dailyTips, format: .currency(code: currencyCode))
                            .font(.headline)
                    }

                    HStack {
                        Text("Items per Guest:")
                            .font(.headline)
                        Spacer()
                        Text(String(format: "%.2f", itemsPerGuest))
                            .font(.headline)
                    }

                    HStack {
                        Text("Items per Hour:")
                            .font(.headline)
                        Spacer()
                        Text(String(format: "%.2f", itemsPerHour))
                            .font(.headline)
                    }
                }
                .padding()
            }
            .navigationTitle("Today's Report")
        }
    }

    private func formatDuration(from start: Date, to end: Date) -> String {
        let interval = end.timeIntervalSince(start)
        let formatter = DateComponentsFormatter()
        formatter.allowedUnits = [.hour, .minute]
        formatter.unitsStyle = .abbreviated
        return formatter.string(from: interval) ?? ""
    }
}

struct DailyReportView_Previews: PreviewProvider {
    static var previews: some View {
        let data: POSData = {
            let d = POSData()
            d.currentDay = Day(startTime: Date(), staff: ["joe"], closedTables: [
                ClosedTable(tableID: 1, guestCount: 4, maxGuests: 5, groupType: .couple, items: [], total: 55.60, taxAmount: 5.00, tipAmount: 10.0, openedAt: Date(), closedAt: Date().addingTimeInterval(3600), customerEmail: nil, paymentMethod: nil),
                ClosedTable(tableID: 3, guestCount: 2, maxGuests: 4, groupType: .business, items: [], total: 32.10, taxAmount: 2.90, tipAmount: 5.0, openedAt: Date(), closedAt: Date().addingTimeInterval(4000), customerEmail: nil, paymentMethod: nil)
            ])
            d.currentDay?.endTime = Date().addingTimeInterval(8 * 3600)
            return d
        }()

        return DailyReportView()
            .environmentObject(data)
    }
}
