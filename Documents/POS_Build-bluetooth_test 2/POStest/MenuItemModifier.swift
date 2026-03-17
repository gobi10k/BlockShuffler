//
//  MenuItemModifier.swift
//  POStest
//
//  Created by Alec Gordon on 14/08/2025.
//

import Foundation

struct MenuItemModifier: Identifiable, Codable, Hashable {
    var id = UUID()
    var name: String
    var priceAdjustment: Double
    var isActive: Bool = true
    
    init(name: String, priceAdjustment: Double) {
        self.name = name
        self.priceAdjustment = priceAdjustment
    }
    
    var displayAdjustment: String {
        if priceAdjustment >= 0 {
            return "+\(priceAdjustment.formatted(.currency(code: "USD")))"
        } else {
            return "\(priceAdjustment.formatted(.currency(code: "USD")))"
        }
    }
}
