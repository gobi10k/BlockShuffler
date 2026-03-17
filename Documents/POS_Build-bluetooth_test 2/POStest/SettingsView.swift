//
//  SettingsView.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import SwiftUI

struct SettingsView: View {
    var body: some View {
        List {
            Section(header: Text("Management")) {
                NavigationLink(destination: EmployeeSettingsView()) {
                    Label("Employees", systemImage: "person.3.fill")
                }
                NavigationLink(destination: MenuSettingsView()) {
                    Label("Menu Items", systemImage: "menucard.fill")
                }
                NavigationLink(destination: GeneralSettingsView()) {
                    Label("General", systemImage: "gearshape.fill")
                }
            }

            Section(header: Text("Layout")) {
                NavigationLink(destination: TableLayoutView()) {
                    Label("Table Layout", systemImage: "square.grid.3x3.fill")
                }
            }
        }
        .listStyle(InsetGroupedListStyle())
        .navigationTitle("Settings")
    }
}

struct SettingsView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            SettingsView()
        }
    }
}
