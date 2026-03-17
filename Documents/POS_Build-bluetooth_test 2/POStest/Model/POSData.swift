//
//  POSData.swift
//  POStest
//
//  Created by Jules on 11/08/2025.
//

import Foundation
import SwiftUI

@MainActor
class POSData: ObservableObject {
    @Published var tables: [Table] = (1...20).map { Table(id: $0) }
    @Published var currentDay: Day?
    @Published var pastDays: [Day] = []
    @Published var employees: [Employee] = []
    @Published var menuItems: [MenuItem] = []
    @Published var taxBands: [TaxBand] = [] // New: Configurable tax bands
    @Published var printers: [POSPrinter] = []

    var hasOpenTables: Bool {
        tables.contains { $0.openedAt != nil }
    }

    init() {
        loadPastDays()
        loadEmployees()
        loadMenuItems()
        loadTaxBands()
        loadPrinters()
    }

    // Generic persistence functions
    private func fileURL(for filename: String) -> URL {
        let documentsDirectory = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
        return documentsDirectory.appendingPathComponent(filename)
    }

    private func save<T: Codable>(_ data: T, to filename: String) {
        do {
            let data = try JSONEncoder().encode(data)
            try data.write(to: fileURL(for: filename), options: .atomic)
        } catch {
            print("Failed to save \(filename): \(error.localizedDescription)")
        }
    }

    private func load<T: Codable>(from filename: String) -> T? {
        do {
            let data = try Data(contentsOf: fileURL(for: filename))
            return try JSONDecoder().decode(T.self, from: data)
        } catch {
            print("Failed to load \(filename).")
            return nil
        }
    }

    // Tax Band Management
    func loadTaxBands() {
        if let loadedBands: [TaxBand] = load(from: "taxBands.json") {
            taxBands = loadedBands
        } else {
            print("Creating default tax bands.")
            taxBands = TaxBand.defaultBands
            saveTaxBands()
        }
    }

    func saveTaxBands() {
        save(taxBands, to: "taxBands.json")
    }

    func addTaxBand(_ band: TaxBand) {
        taxBands.append(band)
        saveTaxBands()
    }

    func removeTaxBand(_ band: TaxBand) {
        taxBands.removeAll { $0.id == band.id }
        saveTaxBands()
    }

    func updateTaxBand(_ band: TaxBand) {
        if let index = taxBands.firstIndex(where: { $0.id == band.id }) {
            taxBands[index] = band
            saveTaxBands()
        }
    }

    // Printer Management
    func loadPrinters() {
        if let loadedPrinters: [POSPrinter] = load(from: "printers.json") {
            printers = loadedPrinters
        } else {
            printers = []
        }
    }

    func savePrinters() {
        save(printers, to: "printers.json")
    }

    func addPrinter(_ printer: POSPrinter) {
        printers.append(printer)
        savePrinters()
    }

    func removePrinter(_ printer: POSPrinter) {
        printers.removeAll { $0.id == printer.id }
        savePrinters()
    }

    // Existing functions
    func loadPastDays() {
        if let loadedDays: [Day] = load(from: "pastDays.json") {
            pastDays = loadedDays
        } else {
            print("Creating sample past days data.")
            let day1 = Day(startTime: Date().addingTimeInterval(-86400), staff: ["joe", "sarah"], closedTables: [
                ClosedTable(tableID: 1, guestCount: 4, maxGuests: 4, groupType: .couple, items: [OrderItem(menuItem: MenuItem(name: "Pizza", price: 12.50, taxCategory: .nonAlcoholic, unitCost: 5.0), timestamp: Date())], total: 55.60, taxAmount: 5.00, tipAmount: 10.0, openedAt: Date(), closedAt: Date().addingTimeInterval(3600), customerEmail: "example@example.com", paymentMethod: "Cash"),
            ])
            pastDays = [day1]
        }
    }

    func savePastDays() {
        save(pastDays, to: "pastDays.json")
    }

    func loadEmployees() {
        if let loadedEmployees: [Employee] = load(from: "employees.json") {
            employees = loadedEmployees
        } else {
            print("Creating sample employee data.")
            employees = [
                Employee(name: "Joe", hourlyRate: 22.50),
                Employee(name: "Sarah", hourlyRate: 23.00),
                Employee(name: "Fred", hourlyRate: 21.75)
            ]
        }
    }

    func saveEmployees() {
        save(employees, to: "employees.json")
    }

    func loadMenuItems() {
        if let loadedItems: [MenuItem] = load(from: "menu.json") {
            menuItems = loadedItems
        } else {
            print("Creating sample menu data.")
            menuItems = [
                // Cocktails
                MenuItem(name: "Negroni", price: 11.00, taxCategory: .alcoholic, unitCost: 3.20, category: .cocktails),
                MenuItem(name: "Espresso Martini", price: 12.00, taxCategory: .alcoholic, unitCost: 3.50, category: .cocktails),
                MenuItem(name: "Old Fashioned", price: 12.00, taxCategory: .alcoholic, unitCost: 3.00, category: .cocktails),
                MenuItem(name: "Aperol Spritz", price: 10.00, taxCategory: .alcoholic, unitCost: 2.80, category: .cocktails),
                MenuItem(name: "Margarita", price: 11.00, taxCategory: .alcoholic, unitCost: 3.00, category: .cocktails),
                // Beer & Cider
                MenuItem(name: "Draught Pils", price: 5.50, taxCategory: .alcoholic, unitCost: 1.50, category: .beer),
                MenuItem(name: "IPA (Bottle)", price: 5.00, taxCategory: .alcoholic, unitCost: 1.80, category: .beer),
                MenuItem(name: "Stout (Bottle)", price: 5.50, taxCategory: .alcoholic, unitCost: 2.00, category: .beer),
                // Wine
                MenuItem(name: "House Red", price: 7.50, taxCategory: .alcoholic, unitCost: 2.50, category: .wine),
                MenuItem(name: "House White", price: 7.50, taxCategory: .alcoholic, unitCost: 2.50, category: .wine),
                MenuItem(name: "Prosecco (Glass)", price: 8.00, taxCategory: .alcoholic, unitCost: 2.80, category: .wine),
                // Spirits
                MenuItem(name: "Gin & Tonic", price: 9.00, taxCategory: .alcoholic, unitCost: 2.50, category: .spirits),
                MenuItem(name: "Rum & Coke", price: 8.50, taxCategory: .alcoholic, unitCost: 2.20, category: .spirits),
                MenuItem(name: "Whisky Neat", price: 9.50, taxCategory: .alcoholic, unitCost: 3.00, category: .spirits),
                // Soft Drinks
                MenuItem(name: "Sparkling Water", price: 3.00, taxCategory: .nonAlcoholic, unitCost: 0.50, category: .softDrinks),
                MenuItem(name: "Cola", price: 3.50, taxCategory: .nonAlcoholic, unitCost: 0.60, category: .softDrinks),
                MenuItem(name: "Fresh OJ", price: 4.00, taxCategory: .nonAlcoholic, unitCost: 1.00, category: .softDrinks),
                // Hot Drinks
                MenuItem(name: "Espresso", price: 3.50, taxCategory: .nonAlcoholic, unitCost: 0.40, category: .hotDrinks),
                MenuItem(name: "Cappuccino", price: 4.50, taxCategory: .nonAlcoholic, unitCost: 0.70, category: .hotDrinks),
                // Food
                MenuItem(name: "Olives", price: 5.00, taxCategory: .nonAlcoholic, unitCost: 1.50, category: .food),
                MenuItem(name: "Charcuterie Board", price: 14.00, taxCategory: .nonAlcoholic, unitCost: 5.00, category: .food),
            ]
        }
    }

    func saveMenuItems() {
        save(menuItems, to: "menu.json")
    }

    func startDay(staff: [String]) {
        currentDay = Day(startTime: Date(), staff: staff)
    }

    func findDayForToday() -> Day? {
        let today = Calendar.current.startOfDay(for: Date())
        return pastDays.first { day in
            Calendar.current.isDate(day.startTime, inSameDayAs: today)
        }
    }

    func reopenDay(_ day: Day) {
        self.currentDay = day
        self.pastDays.removeAll { $0.id == day.id }
    }

    func endDay() {
        if var day = currentDay {
            day.endTime = Date()
            pastDays.append(day)
            savePastDays()
        }
        currentDay = nil
        // Reset all tables
        tables = (1...20).map { Table(id: $0) }
    }

    func openTable(id: Int, guestCount: Int, groupType: GroupType) {
        guard let index = tables.firstIndex(where: { $0.id == id }) else { return }

        tables[index].guestCount = guestCount
        tables[index].groupType = groupType
        tables[index].openedAt = Date()
    }

    func closeTable(id: Int, tip: Double, customerEmail: String?, paymentMethod: String?) {
        guard let index = tables.firstIndex(where: { $0.id == id }),
              let guestCount = tables[index].guestCount,
              let groupType = tables[index].groupType,
              let openedAt = tables[index].openedAt else { return }

        let table = tables[index]

        // Load layout to find maxGuests
        var maxGuests = 0 // Default value
        if let data = UserDefaults.standard.data(forKey: "tableLayoutData"),
           let layouts = try? JSONDecoder().decode([TableLayoutInfo].self, from: data),
           let layout = layouts.first(where: { $0.id == table.id }) {
            maxGuests = layout.maxGuests
        }

        let closedTable = ClosedTable(
            tableID: table.id,
            guestCount: guestCount,
            maxGuests: maxGuests,
            groupType: groupType,
            items: table.items,
            total: table.total,
            taxAmount: table.tax,
            tipAmount: tip,
            openedAt: openedAt,
            closedAt: Date(),
            customerEmail: customerEmail,
            paymentMethod: paymentMethod
        )

        currentDay?.closedTables.append(closedTable)

        // Reset the table
        tables[index].items = []
        tables[index].guestCount = nil
        tables[index].openedAt = nil
        tables[index].groupType = nil
        tables[index].lastOrderItemAt = nil
    }
}
