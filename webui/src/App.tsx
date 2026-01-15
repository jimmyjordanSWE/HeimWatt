import { useEffect, useState } from 'react'
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs"
import { Card, CardContent } from "@/components/ui/card"
import { Activity, Box, Terminal } from "lucide-react"
import PluginManager from './components/PluginManager'
import Dashboard from './components/Dashboard'

function App() {
  const [activeTab, setActiveTab] = useState("dashboard")
  const [sysStatus, setSysStatus] = useState<any>(null)

  useEffect(() => {
    // Simple heartbeat
    const interval = setInterval(async () => {
      try {
        const res = await fetch('/api/status')
        if (res.ok) {
          setSysStatus(await res.json())
        } else {
          setSysStatus(null)
        }
      } catch (e) {
        console.error("Status fetch failed", e)
        setSysStatus(null)
      }
    }, 2000)
    return () => clearInterval(interval)
  }, [])

  return (
    <div className="min-h-screen bg-neutral-950 text-neutral-100 font-sans selection:bg-neutral-800">
      <header className="border-b border-neutral-800 bg-neutral-950/50 backdrop-blur sticky top-0 z-50">
        <div className="container mx-auto px-4 h-16 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 rounded bg-gradient-to-br from-amber-400 to-amber-600 flex items-center justify-center text-black font-bold shadow-[0_0_15px_rgba(245,158,11,0.3)]">
              H
            </div>
            <span className="font-semibold text-lg tracking-tight">HeimWatt</span>
          </div>

          <div className="flex items-center gap-4 text-sm font-medium text-neutral-400">
            <div className="flex items-center gap-2">
              <div className={`w-2 h-2 rounded-full ${sysStatus ? 'bg-green-500 shadow-[0_0_8px_rgba(34,197,94,0.6)] animate-pulse' : 'bg-red-500'}`} />
              {sysStatus ? 'Online' : 'Offline'}
            </div>
            {sysStatus && (
              <span className="bg-neutral-900 border border-neutral-800 px-2 py-1 rounded text-xs font-mono">
                v0.1.0-alpha
              </span>
            )}
          </div>
        </div>
      </header>

      <main className="container mx-auto px-4 py-8">
        <Tabs value={activeTab} onValueChange={setActiveTab} className="space-y-6">
          <TabsList className="bg-neutral-900/50 border border-neutral-800 p-1 h-auto rounded-lg backdrop-blur">
            <TabsTrigger value="dashboard" className="px-4 py-2 gap-2 data-[state=active]:bg-neutral-800 data-[state=active]:text-white">
              <Activity className="w-4 h-4" /> Dashboard
            </TabsTrigger>
            <TabsTrigger value="plugins" className="px-4 py-2 gap-2 data-[state=active]:bg-neutral-800 data-[state=active]:text-white">
              <Box className="w-4 h-4" /> Plugins
            </TabsTrigger>
            <TabsTrigger value="logs" className="px-4 py-2 gap-2 data-[state=active]:bg-neutral-800 data-[state=active]:text-white">
              <Terminal className="w-4 h-4" /> Console
            </TabsTrigger>
          </TabsList>

          <TabsContent value="dashboard" className="space-y-6 animate-in slide-in-from-bottom-2 duration-500">
            <Dashboard status={sysStatus} />
          </TabsContent>

          <TabsContent value="plugins" className="space-y-6 animate-in slide-in-from-bottom-2 duration-500">
            <PluginManager />
          </TabsContent>

          <TabsContent value="logs" className="active:outline-none">
            <Card className="bg-neutral-900 border-neutral-800">
              <CardContent className="p-6 h-[600px] flex items-center justify-center text-neutral-500">
                Log streaming implementing in next iteration...
              </CardContent>
            </Card>
          </TabsContent>
        </Tabs>
      </main>

      {!sysStatus && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center bg-black/60 backdrop-blur-md transition-all duration-500 animate-in fade-in">
          <div className="bg-neutral-900 border border-neutral-800 p-8 rounded-2xl shadow-2xl flex flex-col items-center gap-6 max-w-sm text-center">
            <div className="w-16 h-16 rounded-full bg-red-500/10 flex items-center justify-center border border-red-500/20">
              <div className="w-8 h-8 rounded-full bg-red-500 animate-pulse shadow-[0_0_20px_rgba(239,68,68,0.5)]" />
            </div>
            <div className="space-y-2">
              <h2 className="text-2xl font-bold tracking-tight">Backend Disconnected</h2>
              <p className="text-neutral-400">
                Attempting to reconnect to HeimWatt services. Please ensure the server is running.
              </p>
            </div>
            <div className="flex items-center gap-2 text-xs font-mono text-neutral-500 bg-black/40 px-3 py-1.5 rounded-full border border-neutral-800">
              <span className="w-1 h-1 rounded-full bg-neutral-600 animate-bounce" />
              RETREIVING SYSTEM STATUS
            </div>
          </div>
        </div>
      )}
    </div>
  )
}

export default App
