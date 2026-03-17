//
//  Day.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import Foundation

struct Day: Identifiable, Codable {
    let id: UUID
    let startTime: Date
    var endTime: Date?
    var staff: [String]
    var closedTables: [ClosedTable]

    var date: Date {
        startTime
    }

    var durationInHours: Double {
        guard let endTime = endTime else { return 0 }
        let duration = endTime.timeIntervalSince(startTime) / 3600
        return ceil(duration)
    }

    init(id: UUID = UUID(), startTime: Date, endTime: Date? = nil, staff: [String], closedTables: [ClosedTable] = []) {
        self.id = id
        self.startTime = startTime
        self.endTime = endTime
        self.staff = staff
        self.closedTables = closedTables
    }

    // Updated to use VAT-aware staff cost calculation
    func totalStaffCost(employees: [Employee]) -> Double {
        let staffOnDuty = employees.filter { staff.contains($0.name) }
        return staffOnDuty.reduce(0.0) { $0 + ($1.actualHourlyCost * durationInHours) }
    }
    
    // New method to get staff VAT cost separately
    func totalStaffVATCost(employees: [Employee]) -> Double {
        let staffOnDuty = employees.filter { staff.contains($0.name) }
        return staffOnDuty.reduce(0.0) { $0 + ($1.hourlyVATCost * durationInHours) }
    }
    
    // New method to get base staff cost (excluding VAT)
    func baseStaffCost(employees: [Employee]) -> Double {
        let staffOnDuty = employees.filter { staff.contains($0.name) }
        return staffOnDuty.reduce(0.0) { $0 + ($1.baseHourlyRate * durationInHours) }
    }
}
