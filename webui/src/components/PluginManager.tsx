import { useEffect, useState } from 'react'
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table"
import { Play, Square, Activity } from "lucide-react"

interface Plugin {
    id: string
    pid: number
    type: string
    resource: string
    state: string
    interval: number
    last_run: number
    endpoint?: string
}

const Countdown = ({ lastRun, interval, resource }: { lastRun: number, interval: number, resource: string }) => {
    const [timeLeft, setTimeLeft] = useState(0)

    useEffect(() => {
        const update = () => {
            if (!lastRun || !interval) {
                setTimeLeft(0)
                return
            }
            const nextRun = lastRun + interval
            const now = Math.floor(Date.now() / 1000)
            const remaining = Math.max(0, nextRun - now)
            setTimeLeft(remaining)
        }
        update()
        const i = setInterval(update, 1000)
        return () => clearInterval(i)
    }, [lastRun, interval])

    if (timeLeft <= 0) return <span className="text-amber-400 animate-pulse font-medium">Fetching from {resource}...</span>

    const mins = Math.floor(timeLeft / 60)
    const secs = timeLeft % 60
    return (
        <div className="flex items-center gap-2">
            <span className="text-neutral-500">Fetching from</span>
            <span className="text-neutral-300 font-medium">{resource}</span>
            <span className="text-neutral-500">in</span>
            <span className="font-mono text-emerald-400 bg-emerald-500/10 px-1.5 py-0.5 rounded border border-emerald-500/20">
                {mins}:{secs.toString().padStart(2, '0')}
            </span>
        </div>
    )
}

export default function PluginManager() {
    const [plugins, setPlugins] = useState<Plugin[]>([])
    const [loading, setLoading] = useState(false)
    const [error, setError] = useState<string | null>(null)

    const fetchPlugins = async () => {
        try {
            setLoading(true)
            const res = await fetch('/api/plugins')
            if (!res.ok) throw new Error('Failed to fetch plugins')
            const data = await res.json()
            setPlugins(data)
            setError(null)
        } catch (e) {
            console.error(e)
            setError("Failed to load plugins")
        } finally {
            setLoading(false)
        }
    }

    const controlPlugin = async (id: string, action: string) => {
        try {
            await fetch(`/api/plugins/${id}/${action}`, { method: 'POST' })
            fetchPlugins()
        } catch (e) {
            console.error(e)
        }
    }

    useEffect(() => {
        fetchPlugins()
        const interval = setInterval(fetchPlugins, 3000)
        return () => clearInterval(interval)
    }, [])

    const getStateColor = (state: string) => {
        switch (state) {
            case 'running': return 'bg-emerald-500/15 text-emerald-400 hover:bg-emerald-500/25 border-emerald-500/20'
            case 'stopped': return 'bg-neutral-500/15 text-neutral-400 hover:bg-neutral-500/25 border-neutral-500/20'
            case 'failed': return 'bg-red-500/15 text-red-400 hover:bg-red-500/25 border-red-500/20'
            case 'starting': return 'bg-amber-500/15 text-amber-400 hover:bg-amber-500/25 border-amber-500/20'
            case 'stopping': return 'bg-rose-500/15 text-rose-400 hover:bg-rose-500/25 border-rose-500/20'
            case 'restarting': return 'bg-blue-500/15 text-blue-400 hover:bg-blue-500/25 border-blue-500/20'
            default: return 'bg-neutral-800 text-neutral-400'
        }
    }

    return (
        <Card className="bg-neutral-900/50 backdrop-blur border-neutral-800">
            <CardHeader className="flex flex-row items-center justify-between">
                <CardTitle className="flex items-center gap-2">
                    <Activity className="w-5 h-5 text-neutral-400" />
                    Plugin Management
                </CardTitle>
                {error && <Badge variant="destructive" className="animate-pulse">{error}</Badge>}
            </CardHeader>
            <CardContent>
                <Table>
                    <TableHeader>
                        <TableRow className="border-neutral-800 hover:bg-transparent">
                            <TableHead className="text-neutral-500 w-[400px]">Plugin ID</TableHead>
                            <TableHead className="text-neutral-500">Resource / Target</TableHead>
                            <TableHead className="text-neutral-500">PID</TableHead>
                            <TableHead className="text-neutral-500">Status</TableHead>
                            <TableHead className="text-right text-neutral-500">Actions</TableHead>
                        </TableRow>
                    </TableHeader>
                    <TableBody>
                        {plugins.map((plugin) => (
                            <TableRow key={plugin.id} className="border-neutral-800 hover:bg-neutral-800/50 transition-colors">
                                <TableCell className="font-mono text-neutral-300 font-medium">{plugin.id}</TableCell>
                                <TableCell>
                                    {plugin.type === 'in' ? (
                                        <div className="flex flex-col gap-1.5">
                                            <div className="flex items-center gap-2">
                                                <Badge variant="outline" className="text-[10px] py-0 leading-tight uppercase bg-emerald-500/10 border-emerald-500/20 text-emerald-500 w-fit">
                                                    Input
                                                </Badge>
                                            </div>
                                            {plugin.state === 'running' && (
                                                <div className="text-xs">
                                                    <Countdown lastRun={plugin.last_run} interval={plugin.interval} resource={plugin.resource} />
                                                </div>
                                            )}
                                        </div>
                                    ) : (
                                        <div className="flex flex-col gap-1.5">
                                            <div className="flex items-center gap-2">
                                                <Badge variant="outline" className="text-[10px] py-0 leading-tight uppercase bg-blue-500/10 border-blue-500/20 text-blue-400 w-fit">
                                                    Output
                                                </Badge>
                                                <span className="text-xs text-neutral-400 font-medium">{plugin.resource}</span>
                                            </div>
                                            {plugin.state === 'running' && plugin.endpoint && (
                                                <a
                                                    href={plugin.endpoint}
                                                    target="_blank"
                                                    rel="noreferrer"
                                                    className="text-xs text-blue-400 hover:text-blue-300 flex items-center gap-1 transition-colors group"
                                                >
                                                    <div className="w-1.5 h-1.5 bg-green-500 rounded-full animate-pulse" />
                                                    View API
                                                    <span className="text-neutral-600 group-hover:text-blue-400/50 transition-colors">↗</span>
                                                </a>
                                            )}
                                        </div>
                                    )}
                                </TableCell>
                                <TableCell className="font-mono text-neutral-400 text-xs">{plugin.pid > 0 ? plugin.pid : '—'}</TableCell>
                                <TableCell>
                                    <Badge className={`transition-all duration-300 ${getStateColor(plugin.state)}`}>
                                        {plugin.state}
                                    </Badge>
                                </TableCell>
                                <TableCell className="text-right">
                                    <div className="flex justify-end gap-2">
                                        {plugin.state === 'running' ? (
                                            <>
                                                <Button
                                                    variant="outline" size="sm"
                                                    className="bg-neutral-950 border-neutral-800 hover:bg-neutral-800 hover:text-white h-7 text-xs"
                                                    onClick={() => controlPlugin(plugin.id, 'restart')}
                                                >
                                                    Restart
                                                </Button>
                                                <Button
                                                    variant="destructive" size="sm"
                                                    className="h-7 text-xs bg-red-900/20 hover:bg-red-900/40 text-red-400 border border-red-900/50"
                                                    onClick={() => controlPlugin(plugin.id, 'stop')}
                                                >
                                                    <Square className="w-3 h-3 mr-1" /> Stop
                                                </Button>
                                            </>
                                        ) : (
                                            <Button
                                                variant="default" size="sm"
                                                className="h-7 text-xs bg-emerald-600 hover:bg-emerald-500 text-white"
                                                onClick={() => controlPlugin(plugin.id, 'start')}
                                            >
                                                <Play className="w-3 h-3 mr-1" /> Start
                                            </Button>
                                        )}
                                    </div>
                                </TableCell>
                            </TableRow>
                        ))}
                        {plugins.length === 0 && !loading && (
                            <TableRow>
                                <TableCell colSpan={5} className="text-center h-32 text-neutral-500">No plugins found.</TableCell>
                            </TableRow>
                        )}
                    </TableBody>
                </Table>
            </CardContent>
        </Card >
    )
}
