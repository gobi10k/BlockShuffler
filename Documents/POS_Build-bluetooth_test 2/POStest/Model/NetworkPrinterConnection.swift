import Foundation
import Network

class NetworkPrinterConnection: PrinterConnection {
    private let printer: POSPrinter

    init(printer: POSPrinter) {
        self.printer = printer
    }

    func send(data: Data, completion: @escaping (Bool) -> Void) {
        guard let ipAddress = printer.ipAddress, let printerPort = printer.port else {
            completion(false)
            return
        }

        let host = NWEndpoint.Host(ipAddress)
        let port = NWEndpoint.Port(rawValue: UInt16(printerPort))!

        let connection = NWConnection(host: host, port: port, using: .tcp)

        connection.stateUpdateHandler = { state in
            switch state {
            case .ready:
                connection.send(content: data, completion: .contentProcessed({ error in
                    if let error = error {
                        print("Error sending data: \(error)")
                        connection.cancel()
                        completion(false)
                        return
                    }
                    print("Data sent successfully.")
                    connection.cancel()
                    completion(true)
                }))
            case .failed(let error):
                print("Connection failed: \(error.localizedDescription)")
                connection.cancel()
                completion(false)
            default:
                break
            }
        }

        connection.start(queue: .global())
    }
}
