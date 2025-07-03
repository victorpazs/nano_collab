# 📝 Nano Collab - Editor de Texto Colaborativo

Um editor de texto colaborativo desenvolvido em **C** que utiliza **OpenMPI** para comunicação distribuída e **OpenMP** para paralelização. O sistema permite que múltiplos usuários editem simultaneamente o mesmo documento em tempo real, com sincronização automática e controle de acesso concorrente.

## 🚀 Funcionalidades Principais

### ✨ Edição Colaborativa

- **Edição simultânea**: Múltiplos usuários podem editar o mesmo documento
- **Bloqueio de linhas**: Sistema de controle de acesso que previne conflitos
- **Sincronização em tempo real**: Alterações são propagadas instantaneamente para todos os usuários

### 📋 Recursos do Editor

- **Visualização do documento**: Exibe as primeiras 20 linhas com status de bloqueio
- **Edição por linha**: Usuários podem editar linhas específicas (0-99)
- **Log de alterações**: Todas as modificações são registradas com timestamp
- **Sistema de chat**: Histórico completo de mensagens com timestamps
- **Mensagens privadas**: Comunicação direta entre usuários com lista automática de destinatários

### 🏗️ Arquitetura Distribuída

- **Processo Mestre**: Coordena o documento e gerencia bloqueios
- **Processos Trabalhadores**: Interface de usuário para cada editor
- **Comunicação MPI**: Troca de mensagens entre processos distribuídos
- **Paralelização OpenMP**: Otimização de operações internas

## 📋 Requisitos do Sistema

### Dependências Necessárias

- **GCC**: Compilador C com suporte OpenMP
- **OpenMPI**: Biblioteca para programação paralela distribuída
- **Xterm**: Terminal para interface gráfica individual

### Sistemas Suportados

- Linux (Ubuntu/Debian)
- WSL (Windows Subsystem for Linux)

## 🛠️ Instalação

### Ubuntu/Debian

```bash
# Atualizar repositórios
sudo apt update

# Instalar OpenMPI e dependências
sudo apt install openmpi-bin openmpi-common libopenmpi-dev

# Instalar Xterm para interface
sudo apt install xterm

# Verificar instalação
mpicc --version
```

## 🔧 Compilação e Execução

### 1. Compilar o Projeto

```bash
# Navegar para o diretório do projeto
cd path/to/nano_collab

# Compilar com OpenMPI e OpenMP
mpicc -fopenmp -o editor main.c
```

### 2. Executar o Editor Colaborativo

#### Execução Básica (3 usuários)

```bash
# Executar com 3 processos (1 mestre + 2 usuários)
mpirun -np 3 xterm -e ./editor
```

#### Executar com Mais Usuários

```bash
# Para 5 usuários (1 mestre + 4 usuários)
mpirun -np 5 xterm -e ./editor

# Para 10 usuários (1 mestre + 9 usuários)
mpirun -np 10 xterm -e ./editor
```

## 📖 Como Usar o Editor

### Menu Principal

Cada usuário verá o seguinte menu:

```
[Usuario_X] Menu:
1. Visualizar documento em tempo real
2. Editar linha
3. Enviar mensagem privada
4. Visualizar mensagens recebidas
5. Sair
```

### Operações Disponíveis

#### 1. 👁️ Visualizar Documento em Tempo Real

- **Modo de visualização contínua**: Entra em tela dedicada para visualização
- **Atualização automática**: Documento se atualiza instantaneamente quando outros usuários fazem edições
- **Status de bloqueio**: Mostra quais linhas estão sendo editadas e por qual usuário
- **Mensagens em tempo real**: Recebe e exibe mensagens privadas durante a visualização
- **Controle de tela**: Limpa e re-exibe automaticamente quando há atualizações
- **Sair do modo**: Pressione **'Q' seguido de ENTER** para voltar ao menu principal

#### 2. ✏️ Editar Linha

- Selecione uma linha (0-99) para editar
- Sistema solicita bloqueio ao processo mestre
- Se aprovado, digite o novo conteúdo
- Alteração é sincronizada com todos os usuários

#### 3. 💬 Enviar Mensagem Privada

- **Lista automática**: Exibe automaticamente todos os usuários conectados disponíveis
- **Seleção por rank**: Digite o número do rank do destinatário
- **Confirmação visual**: Mostra confirmação de envio com nome do destinatário
- **Armazenamento**: Mensagens são salvas automaticamente no histórico do destinatário

#### 4. 📨 Visualizar Mensagens Recebidas (Chat)

- **Histórico completo**: Exibe todas as mensagens recebidas de outros usuários
- **Formato de chat**: Interface organizada com timestamp [HH:MM:SS] e identificação do remetente
- **Buffer circular**: Mantém as últimas 50 mensagens automaticamente
- **Mensagens longas**: Quebra automaticamente mensagens que excedem a largura da tela
- **Navegação**: Pressione ENTER para voltar ao menu principal

#### 5. 🚪 Sair

- Encerra a sessão do usuário atual
- Libera todas as linhas bloqueadas pelo usuário
- Quando todos saem, o programa encerra automaticamente

## 📁 Arquivos Gerados

### log_editor.txt

Arquivo de log que registra todas as alterações:

```
[2024-01-15 14:30:25] [Usuario_1] editou a linha 5: "Nova linha de código"
[2024-01-15 14:30:45] [Usuario_2] editou a linha 12: "Comentário adicionado"
```

## 🏗️ Arquitetura Técnica

### Processo Mestre (Rank 0)

- Gerencia o estado global do documento
- Coordena bloqueios de linha
- Distribui atualizações para todos os usuários
- Mantém log de alterações

### Processos Trabalhadores (Rank 1+)

- Interface de usuário individual
- Solicita bloqueios ao mestre
- Recebe atualizações assíncronas
- Processa mensagens privadas

### Comunicação MPI

- **TAG_PEDIDO_BLOQUEIO**: Solicitar acesso exclusivo a linha
- **TAG_RESPOSTA_BLOQUEIO**: Resposta do mestre (aprovado/negado)
- **TAG_ENVIAR_NOVO_TEXTO**: Enviar texto editado
- **TAG_MENSAGEM_PRIVADA**: Comunicação peer-to-peer
- **TAG_ATUALIZACAO**: Sincronização do documento
- **TAG_SAIR/TAG_FINALIZAR**: Controle de sessão

## 🤝 Contribuições

Este projeto foi desenvolvido como demonstração de programação paralela e distribuída usando:

- **MPI (Message Passing Interface)** para comunicação entre processos
- **OpenMP** para paralelização de operações locais
- **Programação concorrente** com controle de acesso
- **Interface de terminal** com códigos ANSI para cores

---

**Desenvolvido em C com OpenMPI + OpenMP | Editor Colaborativo em Tempo Real**
