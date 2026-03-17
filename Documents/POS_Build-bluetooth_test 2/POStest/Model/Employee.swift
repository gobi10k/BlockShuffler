
import Foundation

struct Employee: Identifiable, Codable, Hashable {
    var id = UUID()
    var name: String
    var hourlyRate: Double
    var isRateInclusiveOfVAT: Bool = false // New field for VAT handling
    
    // VAT rate for staff costs (typically 21%)
    private static let staffVATRate: Double = 0.21
    
    // Calculate the actual cost including VAT
    var actualHourlyCost: Double {
        if isRateInclusiveOfVAT {
            return hourlyRate
        } else {
            return hourlyRate * (1 + Employee.staffVATRate)
        }
    }
    
    // Calculate the VAT portion
    var hourlyVATCost: Double {
        if isRateInclusiveOfVAT {
            return hourlyRate - (hourlyRate / (1 + Employee.staffVATRate))
        } else {
            return hourlyRate * Employee.staffVATRate
        }
    }
    
    // Calculate base rate (excluding VAT)
    var baseHourlyRate: Double {
        if isRateInclusiveOfVAT {
            return hourlyRate / (1 + Employee.staffVATRate)
        } else {
            return hourlyRate
        }
    }
}
