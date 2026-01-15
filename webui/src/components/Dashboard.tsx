import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Activity, Server, Database, Clock } from "lucide-react"

export default function Dashboard({ status }: { status: any }) {
    if (!status) return (
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4 animate-pulse">
            {[1, 2, 3, 4].map(i => <div key={i} className="h-32 bg-neutral-900 rounded-lg"></div>)}
        </div>
    )

    return (
        <div className="space-y-6">
            <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
                <Card className="bg-neutral-900/50 border-neutral-800">
                    <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                        <CardTitle className="text-sm font-medium text-neutral-400">System Status</CardTitle>
                        <Activity className="h-4 w-4 text-emerald-500" />
                    </CardHeader>
                    <CardContent>
                        <div className="text-2xl font-bold text-neutral-100">{status.status === 'running' ? 'Healthy' : status.status}</div>
                        <p className="text-xs text-neutral-500 mt-1">All systems operational</p>
                    </CardContent>
                </Card>

                <Card className="bg-neutral-900/50 border-neutral-800">
                    <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                        <CardTitle className="text-sm font-medium text-neutral-400">Active Plugins</CardTitle>
                        <Server className="h-4 w-4 text-blue-500" />
                    </CardHeader>
                    <CardContent>
                        <div className="text-2xl font-bold text-neutral-100">{status.plugins}</div>
                        <p className="text-xs text-neutral-500 mt-1">Loaded modules</p>
                    </CardContent>
                </Card>

                <Card className="bg-neutral-900/50 border-neutral-800">
                    <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                        <CardTitle className="text-sm font-medium text-neutral-400">Active Connections</CardTitle>
                        <Database className="h-4 w-4 text-purple-500" />
                    </CardHeader>
                    <CardContent>
                        <div className="text-2xl font-bold text-neutral-100">{status.connections}</div>
                        <p className="text-xs text-neutral-500 mt-1">IPC / HTTP Clients</p>
                    </CardContent>
                </Card>

                <Card className="bg-neutral-900/50 border-neutral-800">
                    <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                        <CardTitle className="text-sm font-medium text-neutral-400">Uptime</CardTitle>
                        <Clock className="h-4 w-4 text-amber-500" />
                    </CardHeader>
                    <CardContent>
                        <div className="text-2xl font-bold text-neutral-100">{Math.floor(status.uptime)}s</div>
                        <p className="text-xs text-neutral-500 mt-1">Since last restart</p>
                    </CardContent>
                </Card>
            </div>

            {/* Placeholder for Graphs */}
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <Card className="bg-neutral-900/50 border-neutral-800 h-[300px]">
                    <CardHeader>
                        <CardTitle>Energy Flow</CardTitle>
                    </CardHeader>
                    <CardContent className="flex items-center justify-center text-neutral-600">
                        Chart Placeholder
                    </CardContent>
                </Card>
                <Card className="bg-neutral-900/50 border-neutral-800 h-[300px]">
                    <CardHeader>
                        <CardTitle>Price Forecast</CardTitle>
                    </CardHeader>
                    <CardContent className="flex items-center justify-center text-neutral-600">
                        Chart Placeholder
                    </CardContent>
                </Card>
            </div>
        </div>
    )
}
