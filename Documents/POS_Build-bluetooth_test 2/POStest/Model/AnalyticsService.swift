//
//  AnalyticsService.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import Foundation

@MainActor
struct AnalyticsService {
    let posData: POSData

    // MARK: - Revenue Analytics
    func totalRevenue(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        return tables.reduce(0) { $0 + $1.total }
    }
    
    // MARK: - Tax Analytics
    func alcoholicTax(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        return tables.flatMap { $0.items }.reduce(0) { total, item in
            if item.menuItem.taxCategory == .alcoholic {
                let price = item.finalPrice
                let taxAmount = price - (price / (1 + item.menuItem.taxCategory.rate))
                return total + taxAmount
            }
            return total
        }
    }
    
    func nonAlcoholicTax(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        return tables.flatMap { $0.items }.reduce(0) { total, item in
            if item.menuItem.taxCategory == .nonAlcoholic {
                let price = item.finalPrice
                let taxAmount = price - (price / (1 + item.menuItem.taxCategory.rate))
                return total + taxAmount
            }
            return total
        }
    }
    
    func totalTax(from: Date, to: Date) -> Double {
        alcoholicTax(from: from, to: to) + nonAlcoholicTax(from: from, to: to)
    }

    // MARK: - Profit Analytics
    func totalProfit(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        let totalRevenue = tables.reduce(0) { $0 + $1.total }
        let totalCost = tables.flatMap { $0.items }.reduce(0) { $0 + $1.menuItem.unitCost }
        return totalRevenue - totalCost
    }
    
    // Profit from sales only (before staff costs)
    func profitFromSales(from: Date, to: Date) -> Double {
        return totalProfit(from: from, to: to)
    }
    
    // Net profit after staff costs
    func netProfit(from: Date, to: Date) -> Double {
        let salesProfit = profitFromSales(from: from, to: to)
        let staffCost = totalStaffCost(from: from, to: to)
        return salesProfit - staffCost
    }

    func profitMargin(from: Date, to: Date) -> Double {
        let totalRevenue = totalRevenue(from: from, to: to)
        let totalProfit = totalProfit(from: from, to: to)

        if totalRevenue == 0 {
            return 0
        }

        return totalProfit / totalRevenue
    }
    
    func adjustedProfitMargin(from: Date, to: Date) -> Double {
        let revenue = totalRevenue(from: from, to: to)
        let profit = totalProfit(from: from, to: to)
        let staffCost = totalStaffCost(from: from, to: to)
        
        guard revenue > 0 else { return 0 }
        return (profit - staffCost) / revenue
    }

    // MARK: - Staff Cost Analytics (with VAT)
    func totalStaffCost(from: Date, to: Date) -> Double {
        let days = pastDays(from: from, to: to)
        return days.reduce(0) { total, day in
            let duration = day.durationInHours
            let dailyStaffCost = day.staff.reduce(0.0) { dayTotal, employeeName in
                let employee = posData.employees.first { $0.name == employeeName }
                return dayTotal + (employee?.actualHourlyCost ?? 0.0)
            }
            return total + (dailyStaffCost * duration)
        }
    }
    
    func totalStaffVAT(from: Date, to: Date) -> Double {
        let days = pastDays(from: from, to: to)
        return days.reduce(0) { total, day in
            let duration = day.durationInHours
            let dailyVAT = day.staff.reduce(0.0) { dayTotal, employeeName in
                let employee = posData.employees.first { $0.name == employeeName }
                return dayTotal + (employee?.hourlyVATCost ?? 0.0)
            }
            return total + (dailyVAT * duration)
        }
    }

    func totalTips(from: Date, to: Date) -> Double {
        closedTables(from: from, to: to).reduce(0) { $0 + $1.tipAmount }
    }

    func tablesServed(from: Date, to: Date) -> Int {
        closedTables(from: from, to: to).count
    }

    // MARK: - Sales Analytics
    func alcoholicSales(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        return tables.flatMap { $0.items }.reduce(0) { total, item in
            item.menuItem.taxCategory == .alcoholic ? total + item.finalPrice : total
        }
    }
    
    func nonAlcoholicSales(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        return tables.flatMap { $0.items }.reduce(0) { total, item in
            item.menuItem.taxCategory == .nonAlcoholic ? total + item.finalPrice : total
        }
    }
    
    func alcoholToNonAlcoholicRatio(from: Date, to: Date) -> Double {
        let alcoholSales = alcoholicSales(from: from, to: to)
        let nonAlcoholicSales = self.nonAlcoholicSales(from: from, to: to)
        
        guard nonAlcoholicSales > 0 else { return alcoholSales > 0 ? Double.infinity : 0 }
        return alcoholSales / nonAlcoholicSales
    }

    // MARK: - Customer Analytics
    func averageOrderValue(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        guard !tables.isEmpty else { return 0 }
        
        let totalRevenue = tables.reduce(0) { $0 + $1.total }
        return totalRevenue / Double(tables.count)
    }
    
    func averageSpendPerGuest(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        let totalGuests = tables.reduce(0) { $0 + $1.guestCount }
        
        guard totalGuests > 0 else { return 0 }
        
        let totalRevenue = tables.reduce(0) { $0 + $1.total }
        return totalRevenue / Double(totalGuests)
    }
    
    func averageItemsPerGuest(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        let totalItems = tables.reduce(0) { $0 + $1.items.count }
        let totalGuests = tables.reduce(0) { $0 + $1.guestCount }
        
        guard totalGuests > 0 else { return 0 }
        return Double(totalItems) / Double(totalGuests)
    }
    
    func tableEfficiency(from: Date, to: Date) -> Double {
        let tables = closedTables(from: from, to: to)
        guard !tables.isEmpty else { return 0 }
        
        let totalEfficiency = tables.reduce(0.0) { total, table in
            guard table.maxGuests > 0 else { return total }
            return total + (Double(table.guestCount) / Double(table.maxGuests))
        }
        
        return totalEfficiency / Double(tables.count)
    }

    // MARK: - Time Analytics
    func averageTableDuration(from: Date, to: Date) -> TimeInterval {
        let tables = closedTables(from: from, to: to)
        guard !tables.isEmpty else { return 0 }
        
        let totalDuration = tables.reduce(0.0) { total, table in
            return total + table.closedAt.timeIntervalSince(table.openedAt)
        }
        
        return totalDuration / Double(tables.count)
    }

    // MARK: - Helper Methods
    func closedTables(from: Date, to: Date) -> [ClosedTable] {
        posData.pastDays.flatMap { $0.closedTables }.filter { $0.closedAt >= from && $0.closedAt <= to }
    }

    func pastDays(from: Date, to: Date) -> [Day] {
        posData.pastDays.filter { $0.startTime >= from && $0.startTime <= to }
    }

    func dailyTotals(from: Date, to: Date) -> [(Date, Double)] {
        let tables = closedTables(from: from, to: to)
        let groupedByDay = Dictionary(grouping: tables) { (table) -> Date in
            Calendar.current.startOfDay(for: table.closedAt)
        }

        return groupedByDay.map { (date, tables) in
            (date, tables.reduce(0) { $0 + $1.total })
        }.sorted(by: { $0.0 < $1.0 })
    }

    func aggregateSoldItems(for days: [Day]) -> [ItemSale] {
        var itemCounts: [String: Int] = [:]

        for day in days {
            for table in day.closedTables {
                for item in table.items {
                    itemCounts[item.menuItem.name, default: 0] += 1
                }
            }
        }

        return itemCounts.map { ItemSale(name: $0.key, count: $0.value) }
            .sorted { $0.count > $1.count }
    }

    func aggregateGroupTypes(for days: [Day]) -> [GroupTypeCount] {
        var groupCounts: [GroupType: Int] = [:]

        for day in days {
            for table in day.closedTables {
                groupCounts[table.groupType, default: 0] += 1
            }
        }

        return groupCounts.map { GroupTypeCount(groupType: $0.key, count: $0.value) }
            .sorted { $0.count > $1.count }
    }
}
