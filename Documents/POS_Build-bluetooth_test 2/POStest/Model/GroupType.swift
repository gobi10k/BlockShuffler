//
//  GroupType.swift
//  POStest
//
//  Created by Jules on 11/08/2025.
//

import Foundation

enum GroupType: String, CaseIterable, Identifiable, Codable {
    case couple = "Couple"
    case local = "Local"
    case regular = "Regular"
    case tourist = "Tourist"
    case business = "Business"
    case party = "Party"
    case unknown = "Unknown"

    var id: Self { self }
}
