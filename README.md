# PS3i.a

Aplicativo de IA para PlayStation 3, compilado com PSL1GHT.

## Estrutura

- `.github/workflows/build.yml`: compila e empacota o aplicativo.
- `source/main.c`: interface, teclado virtual, controle DualShock 3 e comunicação HTTP com o proxy.
- `pkg/ICON0.PNG`: ícone gerado automaticamente durante a compilação.
- `pkg/USRDIR/EBOOT.BIN`: executável gerado durante a compilação.

## Comunicação com o proxy

O aplicativo mantém o protocolo do projeto:

- `GET /chat?q=pergunta`: envia a pergunta e recebe a resposta.
- `GET /reset`: solicita uma conversa nova.
- Porta TCP: `8080`.

O IP inicial pode ser informado ao iniciar o workflow. Também pode ser alterado no próprio PS3: pressione **Círculo**, digite o IP e confirme com **Start** ou **ENVIAR**. O IP é salvo em `/dev_hdd0/game/CLAUDE001/USRDIR/proxy_ip.txt`.

## Controles

- Direcional: navegar no teclado virtual.
- X: selecionar tecla ou ação.
- Quadrado: apagar caractere.
- Triângulo: inserir espaço.
- Start: enviar pergunta.
- Círculo: editar/cancelar o IP do proxy.
- Select: iniciar conversa nova.
- L1/R1: rolar a resposta.

## Gerar o PKG

1. Abra a aba **Actions** do GitHub.
2. Selecione **Build PS3i.a**.
3. Clique em **Run workflow**.
4. Informe o IP inicial do proxy (ou mantenha o padrão).
5. Aguarde o workflow terminar e baixe o artefato **ps3ia-pkg**.

Um build verde confirma a compilação e o empacotamento no Actions, mas não substitui o teste no PS3 real nem a validação da comunicação com o proxy.
