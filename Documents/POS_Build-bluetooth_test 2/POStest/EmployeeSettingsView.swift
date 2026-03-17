import SwiftUI

struct EmployeeSettingsView: View {
    @EnvironmentObject var posData: POSData
    @State private var newEmployeeName: String = ""
    @State private var newEmployeeRate: String = ""
    @State private var newEmployeeVATInclusive: Bool = false
    @State private var showingAddEmployee = false

    var body: some View {
        VStack(spacing: 24) {
            // Header
            Text("Employee Management")
                .font(.title.weight(.bold))
                .foregroundColor(POSColors.textPrimary)
            
            // Employee List
            if posData.employees.isEmpty {
                VStack(spacing: 16) {
                    Image(systemName: "person.badge.plus")
                        .font(.system(size: 60))
                        .foregroundColor(POSColors.goldPrimary.opacity(0.6))
                    
                    Text("No Staff Added")
                        .font(.title2.weight(.semibold))
                        .foregroundColor(POSColors.textPrimary)
                    
                    Text("Add your first employee to get started")
                        .font(.subheadline)
                        .foregroundColor(POSColors.textSecondary)
                }
                .padding(40)
            } else {
                ScrollView {
                    LazyVStack(spacing: 12) {
                        ForEach(posData.employees) { employee in
                            employeeCard(employee)
                        }
                    }
                    .padding()
                }
            }
            
            Spacer()
            
            // Add Employee Button
            Button("Add Employee") {
                showingAddEmployee = true
            }
            .buttonStyle(PrimaryButtonStyle())
            .padding(.horizontal)
        }
        .padding(20)
        .background(POSColors.backgroundGradient)
        .navigationTitle("Staff Management")
        .navigationBarTitleDisplayMode(.large)
        .sheet(isPresented: $showingAddEmployee) {
            addEmployeeSheet
        }
        .preferredColorScheme(.dark)
    }
    
    private func employeeCard(_ employee: Employee) -> some View {
        VStack(spacing: 12) {
            HStack {
                VStack(alignment: .leading, spacing: 4) {
                    Text(employee.name)
                        .font(.headline.weight(.semibold))
                        .foregroundColor(POSColors.textPrimary)
                    
                    Text(employee.isRateInclusiveOfVAT ? "Rate includes VAT" : "Rate excludes VAT")
                        .font(.caption.weight(.medium))
                        .foregroundColor(employee.isRateInclusiveOfVAT ? POSColors.info : POSColors.warning)
                }
                
                Spacer()
                
                VStack(alignment: .trailing, spacing: 4) {
                    Text(employee.hourlyRate.formatted(.currency(code: "USD")))
                        .font(.title3.weight(.bold))
                        .foregroundColor(POSColors.goldPrimary)
                    
                    Text("per hour")
                        .font(.caption)
                        .foregroundColor(POSColors.textSecondary)
                }
            }
            
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Actual Cost:")
                        .font(.caption.weight(.medium))
                        .foregroundColor(POSColors.textSecondary)
                    Text(employee.actualHourlyCost.formatted(.currency(code: "USD")))
                        .font(.subheadline.weight(.semibold))
                        .foregroundColor(POSColors.textPrimary)
                }
                
                Spacer()
                
                VStack(alignment: .trailing, spacing: 2) {
                    Text("VAT (21%):")
                        .font(.caption.weight(.medium))
                        .foregroundColor(POSColors.textSecondary)
                    Text(employee.hourlyVATCost.formatted(.currency(code: "USD")))
                        .font(.subheadline.weight(.semibold))
                        .foregroundColor(POSColors.warning)
                }
            }
            
            Button("Remove") {
                removeEmployee(employee)
            }
            .buttonStyle(PrimaryButtonStyle(isDestructive: true))
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: POSRadius.medium)
                .fill(POSColors.backgroundMedium)
                .shadow(
                    color: POSShadows.card.color,
                    radius: POSShadows.card.radius,
                    x: POSShadows.card.x,
                    y: POSShadows.card.y
                )
        )
    }
    
    private var addEmployeeSheet: some View {
        NavigationView {
            VStack(spacing: 24) {
                Text("Add New Employee")
                    .font(.title.weight(.bold))
                    .foregroundColor(POSColors.textPrimary)
                
                VStack(alignment: .leading, spacing: 8) {
                    Text("Name")
                        .font(.headline.weight(.medium))
                        .foregroundColor(POSColors.textPrimary)
                    
                    TextField("Employee name", text: $newEmployeeName)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                }
                
                VStack(alignment: .leading, spacing: 8) {
                    Text("Hourly Rate")
                        .font(.headline.weight(.medium))
                        .foregroundColor(POSColors.textPrimary)
                    
                    TextField("0.00", text: $newEmployeeRate)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .keyboardType(.decimalPad)
                }
                
                VStack(alignment: .leading, spacing: 12) {
                    Text("VAT Setting")
                        .font(.headline.weight(.medium))
                        .foregroundColor(POSColors.textPrimary)
                    
                    Toggle("Rate includes VAT (21%)", isOn: $newEmployeeVATInclusive)
                        .font(.subheadline)
                        .foregroundColor(POSColors.textSecondary)
                        .toggleStyle(SwitchToggleStyle(tint: POSColors.goldPrimary))
                    
                    Text(newEmployeeVATInclusive ?
                        "The entered rate already includes VAT. Actual cost will be the same as entered rate." :
                        "VAT will be added to the entered rate. Actual cost will be 21% higher.")
                        .font(.caption)
                        .foregroundColor(POSColors.textMuted)
                        .padding(.top, 4)
                }
                
                Spacer()
                
                HStack(spacing: 16) {
                    Button("Cancel") {
                        resetForm()
                        showingAddEmployee = false
                    }
                    .buttonStyle(SecondaryButtonStyle())
                    
                    Button("Add Employee") {
                        addEmployee()
                    }
                    .buttonStyle(PrimaryButtonStyle())
                    .disabled(newEmployeeName.isEmpty || newEmployeeRate.isEmpty)
                }
            }
            .padding(20)
            .background(POSColors.backgroundGradient)
            .navigationBarHidden(true)
        }
        .preferredColorScheme(.dark)
    }
    
    private func addEmployee() {
        guard !newEmployeeName.isEmpty,
              let rate = Double(newEmployeeRate),
              rate > 0 else { return }
        
        let employee = Employee(
            name: newEmployeeName,
            hourlyRate: rate,
            isRateInclusiveOfVAT: newEmployeeVATInclusive
        )
        
        posData.employees.append(employee)
        posData.saveEmployees()
        
        resetForm()
        showingAddEmployee = false
    }
    
    private func removeEmployee(_ employee: Employee) {
        posData.employees.removeAll { $0.id == employee.id }
        posData.saveEmployees()
    }
    
    private func resetForm() {
        newEmployeeName = ""
        newEmployeeRate = ""
        newEmployeeVATInclusive = false
    }
}

struct EmployeeSettingsView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            EmployeeSettingsView()
                .environmentObject(POSData())
        }
        .preferredColorScheme(.dark)
    }
}
