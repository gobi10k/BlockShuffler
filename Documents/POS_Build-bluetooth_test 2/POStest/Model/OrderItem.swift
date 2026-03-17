//
//  OrderItem.swift
//  POStest
//
//  Created by Jules on 11/08/2025.
//

import Foundation

struct OrderItem: Identifiable, Codable {
    let id: UUID
    let menuItem: MenuItem
    let timestamp: Date
    var appliedModifiers: [MenuItemModifier] = []

    init(id: UUID = UUID(), menuItem: MenuItem, timestamp: Date, appliedModifiers: [MenuItemModifier] = []) {
        self.id = id
        self.menuItem = menuItem
        self.timestamp = timestamp
        self.appliedModifiers = appliedModifiers
    }
    
    // Calculate the final price including modifiers
    var finalPrice: Double {
        let modifierTotal = appliedModifiers.reduce(0) { total, modifier in
            total + modifier.priceAdjustment
        }
        return menuItem.price + modifierTotal
    }
    
    // Get display name with modifiers
    var displayName: String {
        if appliedModifiers.isEmpty {
            return menuItem.name
        } else {
            let modifierNames = appliedModifiers.map { $0.name }.joined(separator: ", ")
            return "\(menuItem.name) (\(modifierNames))"
        }
    }
    
    // Get modifier display for receipts
    var modifierDescription: String {
        if appliedModifiers.isEmpty {
            return ""
        }
        return appliedModifiers.map { "  + \($0.name) \($0.displayAdjustment)" }.joined(separator: "\n")
    }
}
