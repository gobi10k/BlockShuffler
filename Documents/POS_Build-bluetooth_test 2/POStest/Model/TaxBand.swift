//
//  TaxBand.swift
//  POStest
//
//  Created by Alec Gordon on 14/08/2025.
//

import Foundation

struct TaxBand: Identifiable, Codable, Hashable {
    var id = UUID()
    var name: String
    var rate: Double // As decimal (e.g., 0.09 for 9%)
    var color: String = "blue" // For UI color coding
    
    init(name: String, rate: Double, color: String = "blue") {
        self.name = name
        self.rate = rate
        self.color = color
    }
    
    var displayRate: String {
        String(format: "%.1f%%", rate * 100)
    }
    
    // Default tax bands
    static let defaultBands = [
        TaxBand(name: "Non-Alcoholic", rate: 0.09, color: "green"),
        TaxBand(name: "Alcoholic", rate: 0.21, color: "blue")
    ]
}
