import SwiftUI

struct TaxBandManagementView: View {
    @EnvironmentObject var posData: POSData
    @State private var isAddingBand = false

    var body: some View {
        List {
            ForEach(posData.taxBands) { band in
                HStack {
                    Text(band.name)
                    Spacer()
                    Text(band.displayRate)
                }
            }
            .onDelete(perform: deleteBand)
        }
        .navigationTitle("Tax Bands")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button(action: { isAddingBand = true }) {
                    Image(systemName: "plus")
                }
            }
        }
        .sheet(isPresented: $isAddingBand) {
            AddTaxBandView()
        }
    }

    private func deleteBand(at offsets: IndexSet) {
        offsets.forEach { index in
            let band = posData.taxBands[index]
            posData.removeTaxBand(band)
        }
    }
}

struct AddTaxBandView: View {
    @EnvironmentObject var posData: POSData
    @State private var name: String = ""
    @State private var rate: String = ""
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Tax Band Details")) {
                    TextField("Tax Band Name (e.g., VAT)", text: $name)
                    TextField("Rate (e.g., 20 for 20%)", text: $rate)
                        .keyboardType(.decimalPad)
                }

                Section {
                    Button("Add Tax Band") {
                        addBand()
                    }
                    .disabled(name.isEmpty || rate.isEmpty)
                }
            }
            .navigationTitle("Add New Tax Band")
            .navigationBarItems(leading: Button("Cancel") {
                dismiss()
            })
        }
    }

    private func addBand() {
        guard let rateValue = Double(rate) else { return }
        let newBand = TaxBand(name: name, rate: rateValue / 100.0)
        posData.addTaxBand(newBand)
        dismiss()
    }
}

struct TaxBandManagementView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            TaxBandManagementView()
                .environmentObject(POSData())
        }
    }
}
