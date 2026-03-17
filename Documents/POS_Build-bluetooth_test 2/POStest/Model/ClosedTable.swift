//
//  ClosedTable.swift
//  POStest
//
//  Created by Jules on 11/08/2025.
//

import Foundation

struct ClosedTable: Identifiable, Codable {
    let id: UUID
    let tableID: Int
    let guestCount: Int
    let maxGuests: Int
    let groupType: GroupType
    let items: [OrderItem]
    let total: Double
    let taxAmount: Double
    let tipAmount: Double
    let openedAt: Date
    let closedAt: Date
    let customerEmail: String?
    let paymentMethod: String?

    init(id: UUID = UUID(), tableID: Int, guestCount: Int, maxGuests: Int, groupType: GroupType, items: [OrderItem], total: Double, taxAmount: Double, tipAmount: Double, openedAt: Date, closedAt: Date, customerEmail: String?, paymentMethod: String?) {
        self.id = id
        self.tableID = tableID
        self.guestCount = guestCount
        self.maxGuests = maxGuests
        self.groupType = groupType
        self.items = items
        self.total = total
        self.taxAmount = taxAmount
        self.tipAmount = tipAmount
        self.openedAt = openedAt
        self.closedAt = closedAt
        self.customerEmail = customerEmail
        self.paymentMethod = paymentMethod
    }
}
