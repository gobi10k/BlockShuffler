//
//  TableLayoutView.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import SwiftUI

struct TableLayoutView: View {
    @State private var tables: [TableLayoutInfo] = []
    @State private var showingAddTableSheet = false
    @State private var newTableNumber = ""
    @State private var newMaxGuests = ""
    @State private var tapLocation: CGPoint = .zero

    var body: some View {
        VStack {
            Text("Table Layout Editor")
                .font(.largeTitle)
                .padding()

            GeometryReader { geometry in
                ZStack {
                    // This is the canvas area
                    Rectangle()
                        .fill(Color.gray.opacity(0.2))
                        .onTapGesture { location in
                            self.tapLocation = location
                            self.showingAddTableSheet = true
                        }

                    // Render the tables
                    ForEach($tables) { $table in
                        TableRepresentationView(tableInfo: $table)
                    }
                }
                .frame(width: geometry.size.width, height: geometry.size.height)
            }
        }
        .navigationTitle("Edit Table Layout")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button("Save", action: saveLayout)
            }
        }
        .onAppear(perform: loadLayout)
        .sheet(isPresented: $showingAddTableSheet) {
            AddTableView(
                tableNumber: $newTableNumber,
                maxGuests: $newMaxGuests,
                onSave: {
                    saveNewTable()
                    showingAddTableSheet = false
                },
                onCancel: {
                    showingAddTableSheet = false
                }
            )
        }
    }

    private func saveNewTable() {
        guard let tableNumberInt = Int(newTableNumber),
              let maxGuestsInt = Int(newMaxGuests) else {
            // Handle invalid input, maybe show an alert
            return
        }

        let newTable = TableLayoutInfo(
            id: tableNumberInt,
            maxGuests: maxGuestsInt,
            x: tapLocation.x,
            y: tapLocation.y,
            width: 50, // Default width
            height: 50 // Default height
        )

        tables.append(newTable)

        // Reset fields
        newTableNumber = ""
        newMaxGuests = ""
    }

    private func saveLayout() {
        let encoder = JSONEncoder()
        if let encoded = try? encoder.encode(tables) {
            UserDefaults.standard.set(encoded, forKey: "tableLayoutData")
        }
    }

    private func loadLayout() {
        if let data = UserDefaults.standard.data(forKey: "tableLayoutData") {
            let decoder = JSONDecoder()
            if let decoded = try? decoder.decode([TableLayoutInfo].self, from: data) {
                self.tables = decoded
                return
            }
        }
    }
}

// A view that represents a single table in the layout editor
private struct TableRepresentationView: View {
    @Binding var tableInfo: TableLayoutInfo
    @State private var initialSize: CGSize?
    @State private var initialPosition: CGPoint?

    var body: some View {
        ZStack(alignment: .bottomTrailing) {
            // Main table body
            Text("\(tableInfo.id)")
                .font(.headline)
                .frame(maxWidth: .infinity, maxHeight: .infinity)

            // Resize handle
            Circle()
                .fill(Color.white.opacity(0.8))
                .frame(width: 20, height: 20)
                .padding(2)
                .gesture(
                    DragGesture()
                        .onChanged { value in
                            if self.initialSize == nil {
                                self.initialSize = CGSize(width: tableInfo.width, height: tableInfo.height)
                            }
                            guard let initial = self.initialSize else { return }

                            let newWidth = initial.width + value.translation.width
                            let newHeight = initial.height + value.translation.height

                            // Enforce a minimum size
                            tableInfo.width = max(40, newWidth)
                            tableInfo.height = max(40, newHeight)
                        }
                        .onEnded { _ in
                            self.initialSize = nil
                        }
                )
        }
        .frame(width: tableInfo.width, height: tableInfo.height)
        .background(Color.blue)
        .foregroundColor(.white)
        .cornerRadius(8)
        .position(x: tableInfo.x, y: tableInfo.y)
        .gesture(
            DragGesture()
                .onChanged { value in
                    if self.initialPosition == nil {
                        self.initialPosition = CGPoint(x: tableInfo.x, y: tableInfo.y)
                    }
                    guard let initial = self.initialPosition else { return }

                    let newX = initial.x + value.translation.width
                    let newY = initial.y + value.translation.height

                    tableInfo.x = newX
                    tableInfo.y = newY
                }
                .onEnded { _ in
                    self.initialPosition = nil
                }
        )
    }
}

struct TableLayoutView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            TableLayoutView()
        }
    }
}
