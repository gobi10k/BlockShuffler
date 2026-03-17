//
//  AnalyticsModels.swift
//  POStest
//
//  Created by Jules on 13/08/2025.
//

import Foundation

struct ItemSale: Identifiable {
    var id: String { name }
    let name: String
    var count: Int
}

struct GroupTypeCount: Identifiable {
    var id: String { groupType.rawValue }
    let groupType: GroupType
    var count: Int
}

struct DailySale: Identifiable {
    let id = UUID()
    let date: Date
    var totalSales: Double
}
