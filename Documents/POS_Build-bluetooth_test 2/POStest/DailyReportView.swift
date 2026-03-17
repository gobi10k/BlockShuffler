//
//  DailyReportView.swift
//  POStest
//

import SwiftUI

struct DailyReportView: View {
    @EnvironmentObject var posData: POSData
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    private var closedTables: [ClosedTable] {
        posData.currentDay?.closedTables ?? []
    }

    private var dailyTotal: Double    { closedTables.reduce(0) { $0 + $1.total } }
    private var dailyTips: Double     { closedTables.reduce(0) { $0 + $1.tipAmount } }
    private var dailyTax: Double      { closedTables.reduce(0) { $0 + $1.taxAmount } }
    private var tableCount: Int       { closedTables.count }

    private var itemsPerGuest: Double {
        let totalItems  = closedTables.reduce(0) { $0 + $1.items.count }
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

    private var taxBreakdown: [(name: String, amount: Double)] {
        var byName: [String: Double] = [:]
        for table in closedTables {
            for item in table.items {
                let taxRate = item.menuItem.getTaxRate(using: posData.taxBands)
                guard taxRate > 0 else { continue }
                let taxAmount = item.finalPrice - (item.finalPrice / (1 + taxRate))
                let bandName: String
                if let band = posData.taxBands.first(where: { $0.rate == taxRate }) {
                    bandName = "\(band.name) (\(Int(taxRate * 100))%)"
                } else {
                    bandName = "\(item.menuItem.taxCategory.rawValue.capitalized) (\(Int(taxRate * 100))%)"
                }
                byName[bandName, default: 0] += taxAmount
            }
        }
        return byName.map { ($0.key, $0.value) }.sorted { $0.name < $1.name }
    }

    private var staffCost: Double {
        guard let day = posData.currentDay else { return 0 }
        let duration = day.durationInHours
        return day.staff.reduce(0.0) { total, name in
            let rate = posData.employees.first { $0.name == name }?.actualHourlyCost ?? 0
            return total + rate * duration
        }
    }

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                if closedTables.isEmpty {
                    emptyState
                } else {
                    summarySection
                    taxSection
                    tablesSection
                }
            }
            .padding(20)
        }
        .background(POSColors.backgroundGradient)
        .navigationTitle("Today's Report")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
        .toolbarBackground(.visible, for: .navigationBar)
        .toolbarColorScheme(.dark, for: .navigationBar)
        .preferredColorScheme(.dark)
    }

    // MARK: - Empty state

    private var emptyState: some View {
        VStack(spacing: 16) {
            Image(systemName: "tray")
                .font(.system(size: 60))
                .foregroundColor(POSColors.textMuted)
            Text("No tables closed yet today")
                .font(.title3.weight(.semibold))
                .foregroundColor(POSColors.textPrimary)
            Text("Completed tables will appear here.")
                .font(.subheadline)
                .foregroundColor(POSColors.textSecondary)
        }
        .padding(60)
    }

    // MARK: - Summary section

    private var summarySection: some View {
        VStack(spacing: 16) {
            HStack {
                Text("Summary")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
            }

            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
                kpiCard("Revenue", dailyTotal.formatted(.currency(code: currencyCode)),
                        icon: "eurosign.circle.fill", color: POSColors.goldPrimary)
                kpiCard("Tips", dailyTips.formatted(.currency(code: currencyCode)),
                        icon: "heart.fill", color: POSColors.success)
                kpiCard("Tables Served", "\(tableCount)",
                        icon: "table.furniture.fill", color: POSColors.info)
                kpiCard("Items / Guest", String(format: "%.1f", itemsPerGuest),
                        icon: "person.fill", color: POSColors.goldPrimary)
                kpiCard("Items / Hour", String(format: "%.1f", itemsPerHour),
                        icon: "clock.fill", color: POSColors.warning)
                if staffCost > 0 {
                    kpiCard("Staff Cost", staffCost.formatted(.currency(code: currencyCode)),
                            icon: "person.2.fill", color: POSColors.error)
                }
            }
        }
        .padding(20)
        .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))
    }

    private func kpiCard(_ title: String, _ value: String, icon: String, color: Color) -> some View {
        VStack(spacing: 8) {
            HStack {
                Image(systemName: icon)
                    .font(.subheadline)
                    .foregroundColor(color)
                Spacer()
            }
            Text(value)
                .font(.title3.weight(.bold))
                .foregroundColor(POSColors.textPrimary)
                .frame(maxWidth: .infinity, alignment: .leading)
            Text(title)
                .font(.caption.weight(.medium))
                .foregroundColor(POSColors.textSecondary)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: POSRadius.small).fill(POSColors.backgroundDark.opacity(0.5)))
    }

    // MARK: - Tax section

    private var taxSection: some View {
        VStack(spacing: 16) {
            HStack {
                Text("Tax Breakdown")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
            }

            VStack(spacing: 8) {
                ForEach(taxBreakdown, id: \.name) { entry in
                    reportRow(entry.name, entry.amount.formatted(.currency(code: currencyCode)))
                }
                Divider().background(POSColors.goldSubtle.opacity(0.3))
                reportRow("Total Tax", dailyTax.formatted(.currency(code: currencyCode)), bold: true)
            }
        }
        .padding(20)
        .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))
    }

    // MARK: - Tables section

    private var tablesSection: some View {
        VStack(spacing: 16) {
            HStack {
                Text("Closed Tables")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
                Text("\(tableCount) tables")
                    .font(.caption.weight(.medium))
                    .foregroundColor(POSColors.textSecondary)
            }

            VStack(spacing: 1) {
                // Header row
                HStack {
                    Text("Table").frame(width: 52, alignment: .leading)
                    Text("Guests").frame(width: 54, alignment: .center)
                    Text("Duration").frame(maxWidth: .infinity, alignment: .center)
                    Text("Tip").frame(width: 60, alignment: .trailing)
                    Text("Total").frame(width: 72, alignment: .trailing)
                }
                .font(.caption.weight(.semibold))
                .foregroundColor(POSColors.textSecondary)
                .padding(.horizontal, 14)
                .padding(.vertical, 8)
                .background(POSColors.backgroundDark.opacity(0.5))

                ForEach(closedTables) { table in
                    tableRow(table)
                }
            }
            .clipShape(RoundedRectangle(cornerRadius: POSRadius.small))
        }
        .padding(20)
        .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))
    }

    private func tableRow(_ table: ClosedTable) -> some View {
        HStack {
            Text("#\(table.tableID)")
                .font(.body.weight(.semibold))
                .foregroundColor(POSColors.goldPrimary)
                .frame(width: 52, alignment: .leading)

            Text("\(table.guestCount)")
                .font(.body)
                .foregroundColor(POSColors.textSecondary)
                .frame(width: 54, alignment: .center)

            Text(formatDuration(from: table.openedAt, to: table.closedAt))
                .font(.caption)
                .foregroundColor(POSColors.textSecondary)
                .frame(maxWidth: .infinity, alignment: .center)

            Text(table.tipAmount.formatted(.currency(code: currencyCode)))
                .font(.body)
                .foregroundColor(table.tipAmount > 0 ? POSColors.success : POSColors.textMuted)
                .frame(width: 60, alignment: .trailing)

            Text(table.total.formatted(.currency(code: currencyCode)))
                .font(.body.weight(.semibold))
                .foregroundColor(POSColors.textPrimary)
                .frame(width: 72, alignment: .trailing)
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 10)
        .background(POSColors.backgroundLight.opacity(0.04))
    }

    // MARK: - Helpers

    private func reportRow(_ label: String, _ value: String, bold: Bool = false) -> some View {
        HStack {
            Text(label)
                .font(bold ? .headline.weight(.medium) : .body)
                .foregroundColor(bold ? POSColors.textPrimary : POSColors.textSecondary)
            Spacer()
            Text(value)
                .font(bold ? .headline.weight(.semibold) : .body.weight(.medium))
                .foregroundColor(bold ? POSColors.goldPrimary : POSColors.textPrimary)
        }
    }

    private func formatDuration(from start: Date, to end: Date) -> String {
        let fmt = DateComponentsFormatter()
        fmt.allowedUnits = [.hour, .minute]
        fmt.unitsStyle = .abbreviated
        return fmt.string(from: end.timeIntervalSince(start)) ?? ""
    }
}

struct DailyReportView_Previews: PreviewProvider {
    static var previews: some View {
        let data: POSData = {
            let d = POSData()
            d.currentDay = Day(startTime: Date(), staff: ["Anna"], closedTables: [
                ClosedTable(tableID: 1, guestCount: 4, maxGuests: 5, groupType: .couple,
                            items: [], total: 55.60, taxAmount: 5.00, tipAmount: 10.0,
                            openedAt: Date(), closedAt: Date().addingTimeInterval(3600),
                            customerEmail: nil, paymentMethod: nil),
                ClosedTable(tableID: 3, guestCount: 2, maxGuests: 4, groupType: .business,
                            items: [], total: 32.10, taxAmount: 2.90, tipAmount: 5.0,
                            openedAt: Date(), closedAt: Date().addingTimeInterval(4000),
                            customerEmail: nil, paymentMethod: nil)
            ])
            return d
        }()
        return DailyReportView()
            .environmentObject(data)
            .preferredColorScheme(.dark)
    }
}
