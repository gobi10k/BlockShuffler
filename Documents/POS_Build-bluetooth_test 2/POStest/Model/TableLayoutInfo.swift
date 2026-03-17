//
//  TableLayoutInfo.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import Foundation
import CoreGraphics

struct TableLayoutInfo: Identifiable, Codable {
    /// The unique identifier for the table, corresponding to the `Table.id`.
    var id: Int

    /// The maximum number of guests the table can accommodate.
    var maxGuests: Int

    /// The x-coordinate of the table's position on the layout canvas.
    var x: CGFloat

    /// The y-coordinate of the table's position on the layout canvas.
    var y: CGFloat

    /// The width of the table on the layout canvas.
    var width: CGFloat

    /// The height of the table on the layout canvas.
    var height: CGFloat
}
