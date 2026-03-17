//
//  MenuItem.swift
//  POStest
//
//  Created by Jules on 11/08/2025.
//

import Foundation

struct MenuItem: Identifiable, Equatable, Codable {
    var id = UUID()
    var name: String
    var price: Double
    var taxCategory: TaxCategory // Keep for backward compatibility
    var taxBandId: UUID? // New: Reference to configurable tax band
    var unitCost: Double
    var availableModifiers: [MenuItemModifier] = []
    var category: MenuCategory = .other
    var isAvailable: Bool = true // false = 86'd / temporarily unavailable

    init(
        name: String,
        price: Double,
        taxCategory: TaxCategory,
        unitCost: Double,
        taxBandId: UUID? = nil,
        availableModifiers: [MenuItemModifier] = [],
        category: MenuCategory = .other,
        isAvailable: Bool = true
    ) {
        self.name = name
        self.price = price
        self.taxCategory = taxCategory
        self.taxBandId = taxBandId
        self.unitCost = unitCost
        self.availableModifiers = availableModifiers
        self.category = category
        self.isAvailable = isAvailable
    }
    
    // Get tax rate from band or fallback to category
    func getTaxRate(using taxBands: [TaxBand]) -> Double {
        if let bandId = taxBandId,
           let band = taxBands.first(where: { $0.id == bandId }) {
            return band.rate
        }
        return taxCategory.rate // Fallback to legacy system
    }
    
    // Get tax band name
    func getTaxBandName(using taxBands: [TaxBand]) -> String {
        if let bandId = taxBandId,
           let band = taxBands.first(where: { $0.id == bandId }) {
            return band.name
        }
        return taxCategory.rawValue.capitalized // Fallback
    }
    
    // Check if item has any modifiers available
    var hasModifiers: Bool {
        return !availableModifiers.isEmpty
    }
}

