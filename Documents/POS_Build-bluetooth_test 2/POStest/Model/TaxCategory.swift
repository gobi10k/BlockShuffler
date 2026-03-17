//
//  TaxCategory.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import Foundation

enum TaxCategory: String, Codable, CaseIterable {
    case alcoholic
    case nonAlcoholic

    var rate: Double {
        switch self {
        case .alcoholic:
            return 0.21 // 21%
        case .nonAlcoholic:
            return 0.09 // 9%
        }
    }
}
