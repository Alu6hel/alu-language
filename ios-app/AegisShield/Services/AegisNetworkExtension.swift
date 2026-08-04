import NetworkExtension
import Foundation

class AegisFilterDataProvider: NEFilterDataProvider {
    
    override func startFilter(completionHandler: @escaping (Error?) -> Void) {
        // Initialize the Alu Network Engine
        print("Aegis Network Extension Started.")
        completionHandler(nil)
    }
    
    override func stopFilter(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        print("Aegis Network Extension Stopped.")
        completionHandler()
    }
    
    override func handleNewFlow(_ flow: NEFilterFlow) -> NEFilterNewFlowVerdict {
        // We only inspect socket flows (TCP/UDP)
        guard let socketFlow = flow as? NEFilterSocketFlow else {
            return .allow()
        }
        
        // Instruct the OS that we need to inspect the byte streams for this flow
        return .filterDataVerdict(withFilterInbound: true, peekInboundBytes: 4096,
                                  filterOutbound: true, peekOutboundBytes: 4096)
    }
    
    override func handleInboundData(from flow: NEFilterFlow, readBytesStartOffset offset: Int, readBytes: Data) -> NEFilterDataVerdict {
        let isThreat = AluBridge.shared.inspectPacket(rawBytes: readBytes)
        
        if isThreat {
            print("[AEGIS] Inbound Threat Blocked! Connection Dropped.")
            return .drop()
        }
        
        return .allow()
    }
    
    override func handleOutboundData(from flow: NEFilterFlow, readBytesStartOffset offset: Int, readBytes: Data) -> NEFilterDataVerdict {
        let isThreat = AluBridge.shared.inspectPacket(rawBytes: readBytes)
        
        if isThreat {
            print("[AEGIS] Outbound Threat Blocked (Data Exfiltration Prevented)!")
            return .drop()
        }
        
        return .allow()
    }
}
