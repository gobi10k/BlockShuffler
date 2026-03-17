import SwiftUI

struct ModifierManagementView: View {
    @Binding var modifiers: [MenuItemModifier]
    @State private var isAddingModifier = false
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationView {
            VStack {
                List {
                    ForEach(modifiers) { modifier in
                        HStack {
                            Text(modifier.name)
                            Spacer()
                            Text(modifier.displayAdjustment)
                        }
                    }
                    .onDelete(perform: deleteModifier)
                }

                Button("Done") {
                    dismiss()
                }
                .buttonStyle(PrimaryButtonStyle())
                .padding()
            }
            .navigationTitle("Manage Modifiers")
            .navigationBarItems(trailing: Button(action: {
                isAddingModifier = true
            }) {
                Image(systemName: "plus")
            })
            .sheet(isPresented: $isAddingModifier) {
                AddModifierView(modifiers: $modifiers)
            }
        }
    }

    private func deleteModifier(at offsets: IndexSet) {
        modifiers.remove(atOffsets: offsets)
    }
}

struct AddModifierView: View {
    @Binding var modifiers: [MenuItemModifier]
    @State private var name: String = ""
    @State private var priceAdjustment: String = ""
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Modifier Details")) {
                    TextField("Modifier Name (e.g., Extra Cheese)", text: $name)
                    TextField("Price Adjustment (e.g., 1.50 or -0.50)", text: $priceAdjustment)
                        .keyboardType(.decimalPad)
                }

                Section {
                    Button("Add Modifier") {
                        addModifier()
                    }
                    .disabled(name.isEmpty)
                }
            }
            .navigationTitle("Add New Modifier")
            .navigationBarItems(leading: Button("Cancel") {
                dismiss()
            })
        }
    }

    private func addModifier() {
        guard let price = Double(priceAdjustment) else {
            // For simplicity, we'll just ignore invalid input for now.
            // In a real app, you'd show an alert.
            if priceAdjustment.isEmpty {
                let newModifier = MenuItemModifier(name: name, priceAdjustment: 0)
                modifiers.append(newModifier)
                dismiss()
            }
            return
        }
        let newModifier = MenuItemModifier(name: name, priceAdjustment: price)
        modifiers.append(newModifier)
        dismiss()
    }
}
