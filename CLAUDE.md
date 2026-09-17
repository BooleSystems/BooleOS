## Regras de verificação

- NUNCA rodar QEMU automaticamente para verificar boot ou capturar
  screenshot
- NUNCA usar sleeps longos (>2s) em comandos de verificação
- Quando precisar confirmar que o kernel bootou corretamente, ou
  testar qualquer comportamento do shell/editor/syscalls, apenas
  diga o comando exato (ex: "cd tools && make run", depois os
  comandos de shell a digitar) e aguarde o usuário rodar e reportar
  o output. O usuário vai colar o resultado manualmente.
- Depois de qualquer mudança de código, SEMPRE rode "make clean &&
  make" (não só "make") antes de pedir pro usuário testar, e
  confirme no output que os arquivos relevantes foram realmente
  recompilados (procure a linha "CC ../kernel/arquivo.c" no log de
  build). Um "make" sem "clean" pode silenciosamente não recompilar
  se a dependência não estiver corretamente declarada no Makefile,
  fazendo o usuário testar um binário antigo sem saber.
- Nunca assuma que um teste "passou" ou "falhou" sem ver o output
  real colado pelo usuário — não infira do fato de ter compilado sem
  erro que o comportamento em runtime está correto.

## Debug e instrumentação temporária

- Prints de debug adicionados durante investigação DEVEM escrever
  SÓ na serial (serial_putchar/serial_puts/serial_u32/etc), NUNCA em
  VGA (vga_putchar/vga_puts), a menos que explicitamente pedido o
  contrário. O comando "edit" do shell desenha sua própria UI direto
  na tela VGA (barra de status, cursor, texto), e qualquer print de
  debug em VGA se mistura visualmente com a interface e quebra o
  teste. Isso já aconteceu múltiplas vezes nesse projeto.
- Atenção: vga_putchar() já espelha automaticamente cada caractere
  pra serial. NUNCA escreva manualmente em serial DEPOIS de chamar
  uma função que já usa vga_puts()/vga_putchar() internamente — isso
  duplica a saída na serial (já causou bug de "print duplicado" que
  pareceu, por engano, sintoma de syscall rodando duas vezes).
- Sempre que investigar um bug, prefira contadores/instrumentação
  cirúrgica (ex: contador global de chamadas, print do valor exato
  numa função específica) em vez de reescrever lógica "no escuro".
  Reportar os valores encontrados ANTES de aplicar qualquer fix.
- Ao final de uma investigação, remova TODOS os prints de debug
  temporários adicionados durante ela, mas preserve 100% da lógica
  de correção. Rode "make clean && make" pra confirmar que a remoção
  não quebrou o build, e liste explicitamente quais arquivos tiveram
  debug removido.
- Nunca conclua "não há bug" sem antes reproduzir o cenário exato
  reportado (inclusive em disco/estado limpo, se relevante — ex:
  "rm -f build/disk.img && make disk" antes de testar bugs de FAT16).

## Convenções técnicas

- C99 estrito, sem libc (bare metal)
- NASM para assembly
- Sem alocação dinâmica sem passar pelo heap manager já existente
  (kmalloc/kfree)
- Preservar compatibilidade com FAT16/VFS já implementado
- Qualquer mudança em scheduler, interrupts, ou timing de hardware
  (ex: probing de dispositivos ATA) merece atenção redobrada com
  race conditions — nunca assumir que um delay fixo é suficiente
  onde um polling real de status é o correto (ex: bug histórico do
  probe ATA usando delay fixo de 400ns em vez de polling do bit BSY)
- Cuidado com arrays de tamanho fixo em structs de disco (ex:
  name[8]/ext[3] separados em dirents FAT16): nunca indexar além do
  tamanho declarado do array mesmo que o layout de memória pareça
  contíguo — isso é undefined behavior em C e já causou bugs onde o
  print de debug mostrava valores corretos mas a comparação lógica
  falhava (o compilador pode gerar código diferente para leitura
  fora dos limites declarados). Sempre construir um buffer explícito
  do tamanho certo antes de comparar/combinar campos assim.
- Cuidado com lógica duplicada: se uma função de busca/comparação
  (ex: fat16_find) tem uma cópia inline equivalente em outro lugar
  do código (ex: dentro de fat16_write_file), um bug corrigido numa
  cópia não é automaticamente corrigido na outra. Ao corrigir um bug
  desse tipo, procure por padrões idênticos em outros arquivos antes
  de considerar o problema resolvido.
- Funções que fazem I/O de baixo nível (ex: ata_write_sector) devem
  distinguir claramente "operação principal falhou" de "confirmação/
  flush subsequente falhou" — não retornar erro genérico que faça o
  chamador de mais alto nível (fat16_create, etc) pensar que nada foi
  escrito quando na verdade foi.
  
  ## Idioma do código

- A partir da v0.10.1, TODO código novo (comentários, mensagens de
  boot, strings de erro, texto de ajuda, nomes de identificadores)
  deve ser escrito em INGLÊS, sem exceção — não volte a introduzir
  português em nenhum arquivo novo ou editado.
- Isso vale mesmo que um arquivo existente ainda tenha trechos em
  português que não foram tocados numa tarefa específica: ao editar
  qualquer parte de um arquivo, aproveite pra traduzir comentários/
  strings próximos que ainda estejam em português, se estiver dentro
  do escopo razoável da tarefa.
- Prompts do usuário no chat de planejamento continuam em português
  (isso não muda), só o código/output do sistema em si é que deve
  ser 100% inglês daqui pra frente.
  
  ## Documentação

- SEMPRE que uma fase/feature nova for concluída e eu confirmar que
  o teste passou, atualize o README.md como parte da MESMA tarefa
  (não espere um pedido separado): banner de versão, tabela de
  roadmap (marcar fase como Done), seção de syscalls (se adicionou
  syscall nova), lista de arquivos/estrutura (se adicionou arquivo
  novo), e qualquer seção descritiva relevante à feature.
- Isso vale mesmo que o pedido original não mencione o README
  explicitamente — a atualização da documentação é parte implícita
  de "fase concluída", não uma tarefa separada que precisa ser pedida.

## Memória de trabalho (PROGRESS.md)

- TODA sessão nova deve ler PROGRESS.md ANTES de começar qualquer
  tarefa. Ele existe porque a sessão não tem memória do que foi
  decidido ou descoberto em sessões anteriores além do que está no
  código e no git log — PROGRESS.md preenche essa lacuna com
  decisões de arquitetura não-óbvias e dívidas técnicas conhecidas.
- PROGRESS.md deve ser atualizado como parte da MESMA tarefa sempre
  que uma decisão de arquitetura nova for tomada, ou uma dívida
  técnica nova for identificada ou resolvida — mesma convenção já
  aplicada ao README.md pra fases concluídas (não é uma tarefa
  separada que precisa ser pedida).
- PROGRESS.md não deve duplicar conteúdo do README.md (descrições de
  fase, tabelas de syscall, etc.) — só referenciar a seção relevante.
  O README continua sendo a documentação pública do projeto;
  PROGRESS.md é memória de trabalho interna pra sessões futuras.
- Se PROGRESS.md passar de ~200-300 linhas, a próxima sessão que
  notar isso deve consolidar antes de adicionar mais conteúdo:
  dívidas técnicas já resolvidas devem ser removidas (não empilhadas
  como histórico — isso é o que o git log já serve pra registrar), e
  decisões de arquitetura antigas mas ainda relevantes devem ficar
  mais concisas em vez de acumular indefinidamente.

## Convenções de fim de fase (versionamento)

- kernel/version.h é a ÚNICA fonte de verdade pro número de versão
  do projeto (`NULLOS_VERSION`, `NULLOS_PHASE`, `NULLOS_PHASE_DESC`,
  e as strings compostas `NULLOS_BANNER`/`NULLOS_SHORT_BANNER`).
  SEMPRE que uma fase for concluída, o ÚNICO arquivo que precisa ser
  editado pra atualizar a versão é esse header — seguindo o padrão
  `NULLOS_VERSION = "N.0"` / `NULLOS_PHASE = "N"` onde N é o número
  da fase (a versão é EXATAMENTE igual ao número de fase; nunca
  pule, nunca invente um número que não corresponda a uma fase real
  concluída e documentada no README).
- NENHUM outro arquivo deve ter string de versão hardcoded a partir
  de agora — nem kernel/main.c (usa `NULLOS_BANNER` de version.h),
  nem user/shell.c (usa `NULLOS_SHORT_BANNER`, incluído via `-I` no
  Makefile de user/, já que version.h só tem macros de texto, sem
  tipo/função de kernel — seguro de incluir em código de userland),
  nem tools/grub.cfg (gerado em build-time a partir de
  tools/grub.cfg.in + kernel/version.h pela regra "GEN grub.cfg" no
  Makefile — nunca edite build/grub.cfg diretamente, é sobrescrito a
  cada build). Se descobrir uma string de versão hardcoded em
  qualquer lugar novo, ela é um bug de duplicação e deve ser
  substituída por uma referência a version.h (ou, se for um arquivo
  que não é C, gerada em build-time a partir dele), não corrigida
  manualmente toda vez que a versão mudar.
- Antes de finalizar qualquer fase, faça um checklist explícito:
  kernel/version.h atualizado? README atualizado? CHANGELOG
  atualizado? PROGRESS.md atualizado (se aplicável)? syscall.h
  conferido contra a tabela de syscalls do README (ver regra abaixo)?
  PROGRESS.md conferido como fonte primária do número de fase (ver
  regra abaixo)? Só considere a fase "concluída" quando todos esses
  pontos estiverem sincronizados no mesmo commit.
- NUNCA invente um número de versão pra uma fase que não existe ou
  não foi pedida — se não tiver certeza do número de fase correto,
  pergunte antes de decidir, não assuma.

## Convenções de fim de fase (números de syscall)

- kernel/syscall.h é a ÚNICA fonte de verdade pros números de
  syscall (os `#define SYS_*`). Sempre que a tabela de syscalls do
  README.md for escrita ou atualizada, rode um `grep` em syscall.h
  e confira CADA número da tabela contra o valor real do `#define`
  correspondente antes de escrever — nunca reafirme um número "de
  memória" (do que foi discutido na conversa) nem copie de uma
  versão anterior do README sem checar, mesmo que pareça óbvio que
  não mudou.
- Ao concluir qualquer fase que adicione, remova, ou renumere uma
  syscall, o checklist de fim de fase (ver seção de versionamento
  acima) DEVE incluir explicitamente: "grep no syscall.h pra
  confirmar que os números atuais batem com o que o README
  documenta" — isso é tão obrigatório quanto atualizar o banner de
  versão, não uma checagem opcional.
- Se o grep encontrar qualquer divergência (número que mudou, syscall
  nova sem entrada na tabela, ou entrada na tabela sem `#define`
  correspondente), corrija o README imediatamente como parte da
  mesma tarefa — não deixe a divergência documentada "pra depois".

## Convenções de fim de fase (número de fase / roadmap)

- PROGRESS.md é a fonte PRIMÁRIA de qual foi a última fase concluída
  — não README.md, não CHANGELOG.md. Isso é deliberado: PROGRESS.md
  é o arquivo de contexto lido no início de toda sessão nova (ver
  seção "Memória de trabalho" acima), então se ele estiver certo, uma
  sessão futura já começa com a informação correta mesmo que não
  tenha lido README/CHANGELOG ainda.
- Sempre que precisar confirmar ou atualizar "qual é a fase atual" —
  seja pra decidir o próximo número de fase, seja pra revisar se o
  banner/roadmap/changelog estão em dia — confira PRIMEIRO o que
  PROGRESS.md → "Current status" diz, e trate README.md/CHANGELOG.md
  como precisando ser conferidos CONTRA esse valor, nunca o
  contrário. Se os três divergirem, PROGRESS.md é o desempate
  (e a divergência em si já é um bug de sincronização a ser
  corrigido nos outros dois).
- Isso existe porque já aconteceu nesta sessão: o banner de versão
  ficou 4 fases atrasado (preso em "Fase 10" com as Fases 11-13 já
  concluídas) sem que ninguém percebesse até uma auditoria explícita
  pedir a conferência. Uma sessão que confia cegamente no README/
  CHANGELOG sem checar contra PROGRESS.md pode repetir esse mesmo
  erro silenciosamente.