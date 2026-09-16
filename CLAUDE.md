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