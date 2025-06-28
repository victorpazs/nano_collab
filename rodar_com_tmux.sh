#!/bin/bash

SESSION="editor_mpi_tmux"

# Compilar
mpicc -fopenmp -o editor editor_texto_mpi.c

# Encerrar sessão anterior, se houver
tmux kill-session -t $SESSION 2>/dev/null

# Criar sessão nova
tmux new-session -d -s $SESSION "mpirun -np 1 ./editor 0"

# Adicionar painéis para os outros ranks
tmux split-window -h -t $SESSION "mpirun -np 1 ./editor 1"
tmux split-window -v -t $SESSION:0.1 "mpirun -np 1 ./editor 2"

# Ajustar layout
tmux select-layout -t $SESSION tiled

# Iniciar tmux
tmux attach -t $SESSION
