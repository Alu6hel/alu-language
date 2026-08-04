import SwiftUI

struct ThreatLogView: View {
    @EnvironmentObject var viewModel: ScannerViewModel
    
    var body: some View {
        List {
            if viewModel.logs.isEmpty {
                Text("No threat logs available. System is secure.")
                    .foregroundColor(.gray)
                    .listRowBackground(Color.black)
            } else {
                ForEach(viewModel.logs) { log in
                    HStack {
                        VStack(alignment: .leading, spacing: 5) {
                            Text(log.filename.components(separatedBy: "/").last ?? log.filename)
                                .font(.headline)
                                .foregroundColor(.white)
                            
                            Text(log.timestamp, style: .time)
                                .font(.subheadline)
                                .foregroundColor(.gray)
                        }
                        
                        Spacer()
                        
                        if case .threat = log.status {
                            Image(systemName: "exclamationmark.triangle.fill")
                                .foregroundColor(.red)
                        } else {
                            Image(systemName: "checkmark.circle.fill")
                                .foregroundColor(.green)
                        }
                    }
                    .padding(.vertical, 8)
                    .listRowBackground(Color.black)
                }
            }
        }
        .navigationTitle("Threat Logs")
        .navigationBarTitleDisplayMode(.inline)
        .background(Color.black.edgesIgnoringSafeArea(.all))
    }
}
