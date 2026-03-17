//
//  AddTableView.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import SwiftUI

struct AddTableView: View {
    @Binding var tableNumber: String
    @Binding var maxGuests: String
    var onSave: () -> Void
    var onCancel: () -> Void

    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Table Details")) {
                    TextField("Table Number", text: $tableNumber)
                        #if os(iOS)
                        .keyboardType(.numberPad)
                        #endif
                    TextField("Maximum Guests", text: $maxGuests)
                        #if os(iOS)
                        .keyboardType(.numberPad)
                        #endif
                }
            }
            .navigationTitle("Add New Table")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save", action: onSave)
                }
            }
        }
    }
}
