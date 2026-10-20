import './ConnectionAlert.css'

export default function ConnectionAlert() {
  return (
    <div className="connection-alert" role="alert" aria-atomic="true" data-testid="connection-alert">
      <span className="connection-alert__icon" aria-hidden="true">
        <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
          <path d="m3 3 18 18M8.5 16.5a5 5 0 0 1 7 0M5 12a10 10 0 0 1 2-1.3M12 9.5a10 10 0 0 1 7 2.5M2 7.5a16 16 0 0 1 1.5-1M8 4.5a16 16 0 0 1 14 3" />
          <circle cx="12" cy="20" r="1" fill="currentColor" stroke="none" />
        </svg>
      </span>
      <div className="connection-alert__content">
        <p className="connection-alert__title">Conexão com o robô perdida</p>
        <p className="connection-alert__description">
          Nenhuma mensagem recebida há pelo menos 3 segundos. Verifique a conexão do robô.
        </p>
        <span className="connection-alert__status">Aguardando retorno da comunicação</span>
      </div>
    </div>
  )
}
