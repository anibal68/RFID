$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

function Get-WorkingPython {
    $candidates = @()

    $userPython312 = Join-Path $env:LocalAppData "Programs\Python\Python312\python.exe"
    $userPython313 = Join-Path $env:LocalAppData "Programs\Python\Python313\python.exe"

    if (Test-Path $userPython312) { $candidates += $userPython312 }
    if (Test-Path $userPython313) { $candidates += $userPython313 }

    $cmdPython = Get-Command python -ErrorAction SilentlyContinue
    if ($cmdPython) { $candidates += $cmdPython.Source }

    foreach ($candidate in $candidates) {
        try {
            $resolved = & $candidate -c "import sys; print(sys.executable)" 2>$null
            if ($LASTEXITCODE -eq 0 -and $resolved -and ($resolved -notlike "*WindowsApps*")) {
                return $candidate
            }
        } catch {
        }
    }

    return $null
}

function Install-PythonUserScope {
    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $winget) {
        Write-Host "winget nao esta disponivel neste PC." -ForegroundColor Yellow
        return $false
    }

    Write-Host "A instalar Python 3.12 no perfil do utilizador (sem admin)..." -ForegroundColor Cyan
    winget install --id Python.Python.3.12 --scope user --accept-package-agreements --accept-source-agreements

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Falha na instalacao do Python." -ForegroundColor Red
        return $false
    }

    return $true
}

if (-not (Test-Path ".\rfid_simulator.py")) {
    Write-Host "Nao encontrei rfid_simulator.py na pasta atual." -ForegroundColor Red
    Write-Host "Pasta atual: $PWD"
    exit 1
}

$pythonExe = Get-WorkingPython

if (-not $pythonExe) {
    Write-Host "Python nao encontrado (ou apenas alias da Store)." -ForegroundColor Yellow
    $answer = Read-Host "Pretende instalar Python no seu utilizador agora? (s/n)"

    if ($answer -match "^(s|S|y|Y)$") {
        $ok = Install-PythonUserScope
        if (-not $ok) {
            Write-Host "Nao foi possivel instalar automaticamente." -ForegroundColor Red
            Write-Host "Execute manualmente: winget install --id Python.Python.3.12 --scope user"
            exit 1
        }
        Start-Sleep -Seconds 2
        $pythonExe = Get-WorkingPython
    }
}

if (-not $pythonExe) {
    Write-Host "Python ainda nao esta disponivel nesta sessao." -ForegroundColor Yellow
    Write-Host "Feche e volte a abrir o terminal, depois corra novamente este script."
    exit 1
}

Write-Host "A iniciar simulador RFID..." -ForegroundColor Green
& $pythonExe ".\rfid_simulator.py"
