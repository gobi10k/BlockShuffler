//
//  Table.swift
//  POStest
//
//  Created by Jules on 11/08/2025.
//

import Foundation

struct Table: Identifiable {
    let id: Int
    var items: [OrderItem] = []
    var guestCount: Int?
    var openedAt: Date?
    var lastOrderItemAt: Date?
    var groupType: GroupType?

    var total: Double {
        items.reduce(0) { $0 + $1.finalPrice }
    }

    // Calculate tax using configurable tax bands
    func tax(using taxBands: [TaxBand]) -> Double {
        items.reduce(0) { total, item in
            let price = item.finalPrice
            let rate = item.menuItem.getTaxRate(using: taxBands)
            let divisor = 1 + rate
            let taxAmount = (divisor == 0) ? 0 : (price - (price / divisor))
            return total + taxAmount
        }
    }
    
    // Legacy tax calculation (fallback)
    var tax: Double {
        items.reduce(0) { total, item in
            let price = item.finalPrice
            let rate = item.menuItem.taxCategory.rate
            let divisor = 1 + rate
            let taxAmount = (divisor == 0) ? 0 : (price - (price / divisor))
            return total + taxAmount
        }
    }

    // Calculate subtotal using configurable tax bands
    func subtotal(using taxBands: [TaxBand]) -> Double {
        total - tax(using: taxBands)
    }
    
    // Legacy subtotal calculation
    var subtotal: Double {
        total - tax
    }

    var itemCount: Int {
        items.count
    }
}
