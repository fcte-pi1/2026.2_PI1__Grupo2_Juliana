# Frontend

O painel de telemetria usa React 19, TypeScript/JSX e Vite. O componente `App.tsx` carrega o `Dashboard.jsx`, que recebe os quadros pelo WebSocket do backend. Em `src/components/`, `MazeMap` desenha o labirinto e `ConnectionAlert` mostra o estado da conexão.

## Rodar o painel

Com Node.js 24 e npm, execute a partir da raiz do repositório.

```bash
cd src/frontend
npm ci
npm run dev
```

Abra [o painel local](http://localhost:5173). Para receber leituras, inicie também o [backend](../backend/README.md).

### Configurar os endereços da API e do WebSocket

Por padrão, o navegador consulta corridas e logs em `http://localhost:8000` e recebe a telemetria em `ws://localhost:8000/ws/painel`. O arquivo `.env.local` em `src/frontend` é opcional. Para acessar a API em outra máquina, defina nele o endereço alcançável pelo **navegador**, por exemplo:

```dotenv
VITE_API_URL=http://192.168.1.10:8000
VITE_WS_URL=ws://192.168.1.10:8000/ws/painel
```

Para usar o exemplo, a API precisa aceitar conexões nesse endereço. O backend permite requisições HTTP de páginas em `localhost` e `127.0.0.1` por padrão.

Reinicie o Vite após editar o arquivo. Para produção, defina o endereço antes do build, pois o Vite o incorpora aos arquivos gerados.

## Compilar e conferir o build

```bash
npm run build
npm run preview
```

O build verifica o TypeScript e gera os arquivos em `dist/`. O preview serve essa pasta no endereço indicado no terminal.

## Testes

```bash
npm test
npm run coverage
npm run lint
```

O Vitest executa os arquivos `*.test.tsx` e `*.test.jsx` ao lado dos componentes, com Testing Library e jsdom. O comando de cobertura usa o provedor V8, e o lint usa oxlint.
