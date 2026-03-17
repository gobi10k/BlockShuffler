import SwiftUI
import Charts

struct AnalyticsView: View {
    @EnvironmentObject var posData: POSData
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    @State private var selectedDateRange: DateInterval = .init(start: .now.addingTimeInterval(-86400 * 7), end: .now)

    private var analyticsService: AnalyticsService {
        AnalyticsService(posData: posData)
    }
    
    // MARK: - Computed Properties
    private var adjustedProfitMargin: Double {
        let revenue = analyticsService.totalRevenue(from: selectedDateRange.start, to: selectedDateRange.end)
        let profit = analyticsService.totalProfit(from: selectedDateRange.start, to: selectedDateRange.end)
        let staffCost = analyticsService.totalStaffCost(from: selectedDateRange.start, to: selectedDateRange.end)
        
        guard revenue > 0 else { return 0 }
        return (profit - staffCost) / revenue
    }
    
    private var adjustedProfit: Double {
        let profit = analyticsService.totalProfit(from: selectedDateRange.start, to: selectedDateRange.end)
        let staffCost = analyticsService.totalStaffCost(from: selectedDateRange.start, to: selectedDateRange.end)
        return profit - staffCost
    }

    var body: some View {
        ScrollView {
            VStack(spacing: 24) {
                // Header
                headerSection
                
                // Date Range Selection
                dateRangeSection
                
                // Key Metrics Cards
                metricsCardsSection
                
                // Charts Section
                chartsSection
            }
            .padding(20)
        }
        .background(POSColors.backgroundGradient)
        .navigationTitle("Analytics")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
        .toolbarBackground(.visible, for: .navigationBar)
        .toolbarColorScheme(.dark, for: .navigationBar)
        .preferredColorScheme(.dark)
    }
    
    // MARK: - Header Section
    private var headerSection: some View {
        VStack(spacing: 8) {
            Text("Business Analytics")
                .font(.largeTitle.weight(.bold))
                .foregroundColor(POSColors.textPrimary)
            
            Text("Performance insights and trends")
                .font(.subheadline)
                .foregroundColor(POSColors.textSecondary)
        }
    }
    
    // MARK: - Date Range Section
    private var dateRangeSection: some View {
        VStack(spacing: 16) {
            Text("Date Range")
                .font(.headline.weight(.semibold))
                .foregroundColor(POSColors.textPrimary)
                .frame(maxWidth: .infinity, alignment: .leading)
            
            HStack(spacing: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("From")
                        .font(.caption.weight(.medium))
                        .foregroundColor(POSColors.textSecondary)
                    
                    DatePicker(
                        "",
                        selection: Binding(
                            get: { selectedDateRange.start },
                            set: { selectedDateRange.start = $0 }
                        ),
                        displayedComponents: .date
                    )
                    .labelsHidden()
                    .colorScheme(.dark)
                }
                
                VStack(alignment: .leading, spacing: 8) {
                    Text("To")
                        .font(.caption.weight(.medium))
                        .foregroundColor(POSColors.textSecondary)
                    
                    DatePicker(
                        "",
                        selection: Binding(
                            get: { selectedDateRange.end },
                            set: { selectedDateRange.end = $0 }
                        ),
                        displayedComponents: .date
                    )
                    .labelsHidden()
                    .colorScheme(.dark)
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
    
    // MARK: - Metrics Cards Section
    private var metricsCardsSection: some View {
        VStack(spacing: 20) {
            Text("Key Performance Indicators")
                .font(.headline.weight(.semibold))
                .foregroundColor(POSColors.textPrimary)
                .frame(maxWidth: .infinity, alignment: .leading)
            
            // Financial Metrics
            VStack(spacing: 16) {
                Text("Financial Performance")
                    .font(.subheadline.weight(.medium))
                    .foregroundColor(POSColors.goldPrimary)
                    .frame(maxWidth: .infinity, alignment: .leading)
                
                LazyVGrid(columns: [
                    GridItem(.flexible()),
                    GridItem(.flexible())
                ], spacing: 16) {
                    metricCard(
                        title: "Total Revenue",
                        value: analyticsService.totalRevenue(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "dollarsign.circle.fill",
                        color: POSColors.goldPrimary
                    )
                    
                    metricCard(
                        title: "Profit from Sales",
                        value: analyticsService.profitFromSales(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "chart.line.uptrend.xyaxis.circle.fill",
                        color: POSColors.success
                    )
                    
                    metricCard(
                        title: "Net Profit",
                        value: analyticsService.netProfit(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "banknote.fill",
                        color: POSColors.goldPrimary
                    )
                    
                    metricCard(
                        title: "Profit Margin",
                        value: String(format: "%.2f%%", adjustedProfitMargin * 100),
                        icon: "percent.circle.fill",
                        color: POSColors.info
                    )
                    
                    metricCard(
                        title: "Staff Cost (inc. VAT)",
                        value: analyticsService.totalStaffCost(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "person.2.circle.fill",
                        color: POSColors.warning
                    )
                }
            }
            
            // Tax Breakdown
            VStack(spacing: 16) {
                Text("Tax Analysis")
                    .font(.subheadline.weight(.medium))
                    .foregroundColor(POSColors.goldPrimary)
                    .frame(maxWidth: .infinity, alignment: .leading)
                
                LazyVGrid(columns: [
                    GridItem(.flexible()),
                    GridItem(.flexible())
                ], spacing: 16) {
                    metricCard(
                        title: "Non-Alcoholic Tax (9%)",
                        value: analyticsService.nonAlcoholicTax(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "fork.knife.circle.fill",
                        color: POSColors.success
                    )
                    
                    metricCard(
                        title: "Alcohol Tax (21%)",
                        value: analyticsService.alcoholicTax(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "wineglass.fill",
                        color: POSColors.info
                    )
                    
                    metricCard(
                        title: "Total Tax",
                        value: analyticsService.totalTax(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "doc.text.fill",
                        color: POSColors.warning
                    )
                    
                    metricCard(
                        title: "Staff VAT",
                        value: analyticsService.totalStaffVAT(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "person.badge.plus.fill",
                        color: POSColors.error
                    )
                }
            }
            
            // Sales Analytics
            VStack(spacing: 16) {
                Text("Sales Performance")
                    .font(.subheadline.weight(.medium))
                    .foregroundColor(POSColors.goldPrimary)
                    .frame(maxWidth: .infinity, alignment: .leading)
                
                LazyVGrid(columns: [
                    GridItem(.flexible()),
                    GridItem(.flexible())
                ], spacing: 16) {
                    metricCard(
                        title: "Avg Order Value",
                        value: analyticsService.averageOrderValue(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "cart.circle.fill",
                        color: POSColors.goldPrimary
                    )
                    
                    metricCard(
                        title: "Avg Spend/Guest",
                        value: analyticsService.averageSpendPerGuest(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "person.circle.fill",
                        color: POSColors.success
                    )
                    
                    metricCard(
                        title: "Items per Guest",
                        value: String(format: "%.1f", analyticsService.averageItemsPerGuest(from: selectedDateRange.start, to: selectedDateRange.end)),
                        icon: "list.bullet.circle.fill",
                        color: POSColors.info
                    )
                    
                    metricCard(
                        title: "Table Efficiency",
                        value: String(format: "%.1f%%", analyticsService.tableEfficiency(from: selectedDateRange.start, to: selectedDateRange.end) * 100),
                        icon: "table.furniture.fill",
                        color: POSColors.warning
                    )
                }
            }
            
            // Product Mix
            VStack(spacing: 16) {
                Text("Product Mix")
                    .font(.subheadline.weight(.medium))
                    .foregroundColor(POSColors.goldPrimary)
                    .frame(maxWidth: .infinity, alignment: .leading)
                
                LazyVGrid(columns: [
                    GridItem(.flexible()),
                    GridItem(.flexible())
                ], spacing: 16) {
                    metricCard(
                        title: "Alcohol Sales",
                        value: analyticsService.alcoholicSales(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "wineglass.fill",
                        color: POSColors.info
                    )
                    
                    metricCard(
                        title: "Non-Alcoholic Sales",
                        value: analyticsService.nonAlcoholicSales(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "fork.knife",
                        color: POSColors.success
                    )
                    
                    metricCard(
                        title: "Avg Order Value",
                        value: analyticsService.averageOrderValue(from: selectedDateRange.start, to: selectedDateRange.end).formatted(.currency(code: currencyCode)),
                        icon: "cart.circle.fill",
                        color: POSColors.goldPrimary
                    )
                    
                    metricCard(
                        title: "Avg Items/Guest",
                        value: String(format: "%.1f", analyticsService.averageItemsPerGuest(from: selectedDateRange.start, to: selectedDateRange.end)),
                        icon: "list.bullet.circle.fill",
                        color: POSColors.info
                    )
                }
            }
        }
    }
    
    private func metricCard(title: String, value: String, icon: String, color: Color) -> some View {
        VStack(spacing: 12) {
            HStack {
                Image(systemName: icon)
                    .font(.title2)
                    .foregroundColor(color)
                
                Spacer()
                
                Text(value)
                    .font(.title3.weight(.bold))
                    .foregroundColor(POSColors.textPrimary)
            }
            
            Text(title)
                .font(.caption.weight(.medium))
                .foregroundColor(POSColors.textSecondary)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(16)
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
            revenueAndProfitChart
            staffCostChart
        }
    }

    private var revenueAndProfitChart: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Revenue & Profit Trends")
                .font(.headline.weight(.semibold))
                .foregroundColor(POSColors.textPrimary)

            Chart {
                ForEach(analyticsService.dailyTotals(from: selectedDateRange.start, to: selectedDateRange.end), id: \.0) { date, total in
                    BarMark(
                        x: .value("Date", date, unit: .day),
                        y: .value("Revenue", total)
                    )
                    .foregroundStyle(POSColors.goldPrimary)
                }
            }
            .frame(height: 300)
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
    }

    private var staffCostChart: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Daily Staff Costs")
                .font(.headline.weight(.semibold))
                .foregroundColor(POSColors.textPrimary)

            Chart {
                ForEach(analyticsService.pastDays(from: selectedDateRange.start, to: selectedDateRange.end)) { day in
                    BarMark(
                        x: .value("Date", day.startTime, unit: .day),
                        y: .value("Cost", day.totalStaffCost(employees: posData.employees))
                    )
                    .foregroundStyle(POSColors.warning)
                }
            }
            .frame(height: 250)
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
    }
}

struct AnalyticsView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            AnalyticsView()
                .environmentObject(POSData())
                .preferredColorScheme(.dark)
        }
    }
}
