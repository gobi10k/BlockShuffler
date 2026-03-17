import SwiftUI
import Charts

struct PastReportsView: View {
    @EnvironmentObject var posData: POSData
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    var body: some View {
        NavigationStack {
            List(posData.pastDays) { day in
                NavigationLink(destination: DayDetailView(day: day)) {
                    HStack {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(day.date, style: .date)
                                .font(.headline.weight(.medium))
                                .foregroundColor(POSColors.textPrimary)
                            
                            Text("\(day.closedTables.count) tables served")
                                .font(.caption)
                                .foregroundColor(POSColors.textSecondary)
                        }
                        
                        Spacer()
                        
                        VStack(alignment: .trailing, spacing: 4) {
                            Text(day.closedTables.reduce(0) { $0 + $1.total }, format: .currency(code: currencyCode))
                                .font(.headline.weight(.semibold))
                                .foregroundColor(POSColors.goldPrimary)
                            
                            Text("Revenue")
                                .font(.caption)
                                .foregroundColor(POSColors.textSecondary)
                        }
                    }
                    .padding(.vertical, 4)
                }
            }
            .listStyle(PlainListStyle())
            .background(POSColors.backgroundDark)
            .navigationTitle("Past Reports")
            .navigationBarTitleDisplayMode(.large)
            .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbarColorScheme(.dark, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .primaryAction) {
                    NavigationLink(destination: AnalyticsView()) {
                        Label("Analytics", systemImage: "chart.bar.xaxis")
                            .foregroundColor(POSColors.goldPrimary)
                    }
                }
            }
            .overlay {
                if posData.pastDays.isEmpty {
                    VStack(spacing: 16) {
                        Image(systemName: "doc.text.magnifyingglass")
                            .font(.system(size: 60))
                            .foregroundColor(POSColors.goldPrimary.opacity(0.6))
                        
                        Text("No Past Reports")
                            .font(.title2.weight(.semibold))
                            .foregroundColor(POSColors.textPrimary)
                        
                        Text("Complete some sales to see reports here")
                            .font(.subheadline)
                            .foregroundColor(POSColors.textSecondary)
                    }
                }
            }
        }
        .preferredColorScheme(.dark)
    }
}

struct DayDetailView: View {
    let day: Day
    @EnvironmentObject var posData: POSData
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    private var dailyTotal: Double {
        day.closedTables.reduce(0) { $0 + $1.total }
    }

    private var dailyTax: Double {
        day.closedTables.reduce(0) { $0 + $1.taxAmount }
    }

    private var dailyTips: Double {
        day.closedTables.reduce(0) { $0 + $1.tipAmount }
    }
    
    private var dailyProfitFromSales: Double {
        let revenue = dailyTotal
        let itemCosts = day.closedTables.flatMap { $0.items }.reduce(0) { $0 + $1.menuItem.unitCost }
        return revenue - itemCosts
    }
    
    private var dailyNetProfit: Double {
        return dailyProfitFromSales - staffCost
    }

    private var analyticsService: AnalyticsService {
        AnalyticsService(posData: posData)
    }

    private var itemSalesData: [ItemSale] {
        analyticsService.aggregateSoldItems(for: [day])
    }

    private var groupTypeData: [GroupTypeCount] {
        analyticsService.aggregateGroupTypes(for: [day])
    }

    private var taxBreakdown: [TaxBand: Double] {
        var breakdown: [TaxBand: Double] = [:]

        for table in day.closedTables {
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
        let duration = day.durationInHours
        let totalRate = day.staff.reduce(0.0) { total, employeeName in
            let employeeRate = posData.employees.first { $0.name == employeeName }?.hourlyRate ?? 0.0
            return total + employeeRate
        }
        return totalRate * duration
    }

    private var itemsPerGuest: Double {
        let totalItems = day.closedTables.reduce(0) { $0 + $1.items.count }
        let totalGuests = day.closedTables.reduce(0) { $0 + $1.guestCount }
        guard totalGuests > 0 else { return 0 }
        return Double(totalItems) / Double(totalGuests)
    }

    private var itemsPerHour: Double {
        let totalItems = day.closedTables.reduce(0) { $0 + $1.items.count }
        let duration = day.durationInHours
        guard duration > 0 else { return 0 }
        return Double(totalItems) / duration
    }

    var body: some View {
        ScrollView {
            VStack(spacing: 24) {
                // Tables Data Table
                tablesTableSection
                
                // Summary Statistics
                summarySection
                
                // Analytics Charts
                chartsSection
            }
            .padding(20)
        }
        .background(POSColors.backgroundGradient)
        .navigationTitle(day.date.formatted(date: .abbreviated, time: .omitted))
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
        .toolbarBackground(.visible, for: .navigationBar)
        .toolbarColorScheme(.dark, for: .navigationBar)
        .preferredColorScheme(.dark)
    }
    
    // MARK: - Tables Table Section
    private var tablesTableSection: some View {
        VStack(spacing: 16) {
            HStack {
                Text("Tables Served")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
            }
            
            if #available(iOS 16.0, *) {
                SwiftUI.Table(day.closedTables) {
                    TableColumn("Table") { table in
                        Text("\(table.tableID)")
                            .foregroundColor(POSColors.textPrimary)
                    }
                    TableColumn("Guests") { table in
                        Text("\(table.guestCount)")
                            .foregroundColor(POSColors.textSecondary)
                    }
                    TableColumn("Efficiency") { table in
                        if table.maxGuests > 0 {
                            Text(Double(table.guestCount) / Double(table.maxGuests), format: .percent)
                                .foregroundColor(POSColors.info)
                        } else {
                            Text("N/A")
                                .foregroundColor(POSColors.textMuted)
                        }
                    }
                    TableColumn("Duration") { table in
                        Text(formatDuration(from: table.openedAt, to: table.closedAt))
                            .foregroundColor(POSColors.textSecondary)
                    }
                    TableColumn("Tax") { table in
                        Text(table.taxAmount, format: .currency(code: currencyCode))
                            .foregroundColor(POSColors.warning)
                    }
                    TableColumn("Tip") { table in
                        Text(table.tipAmount, format: .currency(code: currencyCode))
                            .foregroundColor(POSColors.success)
                    }
                    TableColumn("Total") { table in
                        Text(table.total, format: .currency(code: currencyCode))
                            .foregroundColor(POSColors.goldPrimary)
                            .fontWeight(.semibold)
                    }
                }
                .background(POSColors.backgroundMedium)
                .cornerRadius(POSRadius.medium)
            } else {
                // Fallback for earlier iOS versions
                VStack(spacing: 8) {
                    ForEach(day.closedTables) { table in
                        tableRowFallback(table)
                    }
                }
            }
        }
        .padding(20)
        .background(
            RoundedRectangle(cornerRadius: POSRadius.medium)
                .fill(POSColors.backgroundMedium)
                .shadow(
                    color: POSShadows.card.color,
                    radius: POSShadows.card.radius,
                    x: POSShadows.card.x,
                    y: POSShadows.card.y
                )
        )
    }
    
    private func tableRowFallback(_ table: ClosedTable) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Table \(table.tableID)")
                    .font(.headline.weight(.medium))
                    .foregroundColor(POSColors.textPrimary)
                
                Spacer()
                
                Text(table.total, format: .currency(code: currencyCode))
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.goldPrimary)
            }
            
            HStack {
                Text("Guests: \(table.guestCount)")
                    .font(.subheadline)
                    .foregroundColor(POSColors.textSecondary)
                
                Spacer()
                
                if table.maxGuests > 0 {
                    Text("Efficiency: \(Double(table.guestCount) / Double(table.maxGuests), format: .percent)")
                        .font(.subheadline)
                        .foregroundColor(POSColors.info)
                }
            }
            
            Text("Duration: \(formatDuration(from: table.openedAt, to: table.closedAt))")
                .font(.caption)
                .foregroundColor(POSColors.textMuted)
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: POSRadius.small)
                .fill(POSColors.backgroundLight.opacity(0.3))
        )
    }
    
    // MARK: - Summary Section
    private var summarySection: some View {
        VStack(spacing: 16) {
            HStack {
                Text("Daily Summary")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
            }
            
            LazyVGrid(columns: [
                GridItem(.flexible()),
                GridItem(.flexible())
            ], spacing: 16) {
                summaryCard(title: "Total Revenue", value: dailyTotal.formatted(.currency(code: currencyCode)), color: POSColors.goldPrimary)
                ForEach(taxBreakdown.sorted(by: { $0.key.rate < $1.key.rate }), id: \.key.id) { band, amount in
                    summaryCard(title: "Tax (\(band.name))", value: amount.formatted(.currency(code: currencyCode)), color: POSColors.warning)
                }
                summaryCard(title: "Tips Earned", value: dailyTips.formatted(.currency(code: currencyCode)), color: POSColors.success)
                summaryCard(title: "Profit from Sales", value: dailyProfitFromSales.formatted(.currency(code: currencyCode)), color: POSColors.info)
                summaryCard(title: "Net Profit", value: dailyNetProfit.formatted(.currency(code: currencyCode)), color: dailyNetProfit >= 0 ? POSColors.success : POSColors.error)
                summaryCard(title: "Items/Guest", value: String(format: "%.2f", itemsPerGuest), color: POSColors.textSecondary)
                summaryCard(title: "Items/Hour", value: String(format: "%.2f", itemsPerHour), color: POSColors.textSecondary)
            }
        }
    }
    
    private func summaryCard(title: String, value: String, color: Color) -> some View {
        VStack(spacing: 8) {
            Text(value)
                .font(.title2.weight(.bold))
                .foregroundColor(color)
            
            Text(title)
                .font(.caption.weight(.medium))
                .foregroundColor(POSColors.textSecondary)
                .multilineTextAlignment(.center)
        }
        .padding(16)
        .frame(maxWidth: .infinity)
        .background(
            RoundedRectangle(cornerRadius: POSRadius.medium)
                .fill(POSColors.backgroundMedium)
                .shadow(
                    color: POSShadows.card.color,
                    radius: POSShadows.card.radius,
                    x: POSShadows.card.x,
                    y: POSShadows.card.y
                )
        )
    }
    
    // MARK: - Charts Section
    private var chartsSection: some View {
        VStack(spacing: 24) {
            // Sold Items Chart
            VStack(alignment: .leading, spacing: 16) {
                Text("Top Selling Items")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)

                Chart(itemSalesData.prefix(5)) { item in
                    BarMark(
                        x: .value("Count", item.count),
                        y: .value("Item", item.name)
                    )
                    .foregroundStyle(POSColors.goldPrimary)
                }
                .frame(height: 200)
                .chartBackground { _ in
                    POSColors.backgroundMedium.opacity(0.3)
                }
            }
            .padding(20)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .fill(POSColors.backgroundMedium)
                    .shadow(
                        color: POSShadows.card.color,
                        radius: POSShadows.card.radius,
                        x: POSShadows.card.x,
                        y: POSShadows.card.y
                    )
            )

            // Group Types Chart
            VStack(alignment: .leading, spacing: 16) {
                Text("Customer Types")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)

                Chart(groupTypeData) { group in
                    SectorMark(
                        angle: .value("Count", group.count),
                        innerRadius: .ratio(0.618),
                        angularInset: 2.0
                    )
                    .foregroundStyle(by: .value("Group", group.groupType.rawValue))
                    .annotation(position: .overlay) {
                        Text("\(group.count)")
                            .font(.caption.weight(.bold))
                            .foregroundStyle(POSColors.textPrimary)
                    }
                }
                .frame(height: 200)
                .chartForegroundStyleScale([
                    "Couple": POSColors.goldPrimary,
                    "Local": POSColors.success,
                    "Regular": POSColors.info,
                    "Tourist": POSColors.warning,
                    "Business": POSColors.error,
                    "Party": Color.purple,
                    "Unknown": POSColors.textMuted
                ])
            }
            .padding(20)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .fill(POSColors.backgroundMedium)
                    .shadow(
                        color: POSShadows.card.color,
                        radius: POSShadows.card.radius,
                        x: POSShadows.card.x,
                        y: POSShadows.card.y
                    )
            )
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

struct PastReportsView_Previews: PreviewProvider {
    static var previews: some View {
        let data: POSData = {
            let d = POSData()
            let day1 = Day(startTime: Date().addingTimeInterval(-86400), endTime: Date().addingTimeInterval(-86400 + 8 * 3600), staff: ["Alice"], closedTables: [
                ClosedTable(tableID: 1, guestCount: 4, maxGuests: 5, groupType: .couple, items: [], total: 55.60, taxAmount: 5.00, tipAmount: 10.0, openedAt: Date(), closedAt: Date().addingTimeInterval(3600), customerEmail: nil, paymentMethod: nil),
                ClosedTable(tableID: 3, guestCount: 2, maxGuests: 2, groupType: .business, items: [], total: 32.10, taxAmount: 2.90, tipAmount: 5.0, openedAt: Date(), closedAt: Date().addingTimeInterval(4000), customerEmail: nil, paymentMethod: nil)
            ])
            d.pastDays = [day1]
            d.employees = [Employee(name: "Alice", hourlyRate: 20)]
            return d
        }()

        return NavigationView {
            PastReportsView()
                .environmentObject(data)
                .preferredColorScheme(.dark)
        }
    }
}
