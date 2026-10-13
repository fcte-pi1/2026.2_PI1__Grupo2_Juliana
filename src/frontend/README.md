# Frontend

Dashboard de telemetria do Micromouse em [React](https://react.dev/) + TypeScript, criado com [Vite](https://vite.dev/).

## Requisitos
- Node.js 24

## Instalação
```bash
cd src/frontend
npm install
```

## Rodar
```bash
npm run dev
```
O site abre em http://localhost:5173.

## Testar
```bash
npm test            # roda os testes (Vitest)
npm run coverage    # roda os testes e mostra a cobertura
npm run lint        # verifica o código (oxlint)
npm run build       # gera a versão de produção em dist/
```
Os testes ficam ao lado do componente, com o nome `*.test.tsx`. O GitHub Actions roda lint, build e cobertura em todo Pull Request.
