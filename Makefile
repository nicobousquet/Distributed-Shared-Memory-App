# Makefile du répertoire source

# Définissez les répertoires Phase1 et Phase2
PHASE1_DIR = Phase1
PHASE2_DIR = Phase2

# Par défaut, exécutez les makefiles dans les répertoires Phase1 et Phase2
all: phase1 phase2

phase1:
	@$(MAKE) -C $(PHASE1_DIR)

phase2:
	@$(MAKE) -C $(PHASE2_DIR)

# Cible pour nettoyer les répertoires Phase1 et Phase2
clean:
	@$(MAKE) -C $(PHASE1_DIR) clean
	@$(MAKE) -C $(PHASE2_DIR) clean
	
veryclean:
	@$(MAKE) -C $(PHASE1_DIR) veryclean
	@$(MAKE) -C $(PHASE2_DIR) clean

.PHONY: all phase1 phase2 clean

