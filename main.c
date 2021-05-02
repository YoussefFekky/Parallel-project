// Finalized on 12/6/2019 by Youssef Hassan

#include "mpi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char* substring(char s[], int p, int l) {
	int c = 0;
	char* sub = malloc((l+1) * sizeof(char));
	while (c < l) {
		sub[c] = s[p + c];
		c++;
	}
	sub[c] = '\0';
	return sub;
}

void generatefile(char *filename, int ncandidates, int nvoters)
{
	FILE* fp = fopen(filename, "w");
	fprintf(fp, "%d\n", ncandidates);
	fprintf(fp, "%d\n", nvoters);
	int i;
	for (i = 0; i < nvoters; i++) {
		int j, *set = malloc(ncandidates * sizeof(int));
		memset(set, 0, ncandidates * sizeof(int));
		for (j = 0; j < ncandidates; j++) {
			if (j == ncandidates - 1) {
				if (i == nvoters - 1) {
					int k;
					for (k = 0; k < ncandidates; k++) {
						if (!set[k]) {
							fprintf(fp, "%d", k + 1);
						}
					}
				}
				else
				{
					int k;
					for (k = 0; k < ncandidates; k++) {
						if (!set[k]) {
							fprintf(fp, "%d\n", k + 1);
						}
					}
				}
			}
			else
			{
				int temp = rand() % ncandidates;
				if (set[temp]) {
					j--;
					continue;
				}
				else
				{
					set[temp] = 1;
					fprintf(fp, "%d ", temp + 1);
				}
			}
		}
		free(set);
	}
	fclose(fp);
}

int main(int argc, char* argv) {
	int rank, np, ncandidates = 3, nvoters, portionSize, remainder, lineLen, start, i, extraround = 1;
	int *localCandidateScore, *globalCandidateScore = NULL, **remainderVotes = NULL;
	char inputline[100];
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &np);
	if (rank == 0) {
		char choice;
		printf("Generate file?(y/n)");
		fflush(stdout);
		scanf("%c", &choice);
		if (choice == 'y') {
			printf("Enter the number of candidates: ");
			fflush(stdout);
			scanf("%d", &ncandidates);
			printf("Enter the number of voters: ");
			fflush(stdout);
			scanf("%d", &nvoters);
			srand(time(NULL));
			generatefile("Votes.txt", ncandidates, nvoters);
		}	
	}
	FILE* fp = fopen("Votes.txt", "r");
	while (fp == NULL && rank != 0) {
		fp = fopen("Votes.txt", "r");
	}
	if (rank == 0) {
		if (fp == NULL) {
			printf("The file doesn't exist!");
			MPI_Abort(MPI_COMM_WORLD, 1);
			return 0;
		}
		fgets(inputline, 100, fp);
		sscanf(inputline, "%d", &ncandidates);
		fgets(inputline, 100, fp);
		sscanf(inputline, "%d", &nvoters);
		portionSize = nvoters / np;
		remainder = nvoters - (portionSize * np);
		start = ftell(fp);
		fgets(inputline, 100, fp);
		lineLen = strlen(inputline)+1;
		globalCandidateScore = malloc(ncandidates * sizeof(int));
		extraround = 1;
	}
	MPI_Bcast(&start, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&lineLen, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&portionSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&ncandidates, 1, MPI_INT, 0, MPI_COMM_WORLD);
	localCandidateScore = malloc(ncandidates * sizeof(int));
	memset(localCandidateScore, 0, ncandidates * sizeof(int));
	fseek(fp, start + rank * lineLen * portionSize, 0);
	int **votes = malloc(portionSize * sizeof(int*));
	for (i = 0; i < portionSize; i++) {
		votes[i] = malloc(ncandidates * sizeof(int));
		fgets(inputline, 100, fp);
		int j, k, pos = 0;
		for (j = 0, k = 0; j < ncandidates; k++) {
			if (inputline[k] == ' ' || inputline[k] == '\n' || inputline[k] == '\0') {
				sscanf(substring(inputline, pos, k - pos), "%d", &votes[i][j]);
				j++;
				pos = k+1;
			}
		}
	}
	for (i = 0; i < portionSize; i++) {
		localCandidateScore[votes[i][0] - 1]++;
	}
	MPI_Reduce(localCandidateScore, globalCandidateScore, ncandidates, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	if (rank == 0) {
		if (remainder != 0) {
			fseek(fp, start + np * lineLen * portionSize, 0);
			remainderVotes = malloc(remainder * sizeof(int*));
			for (i = 0; i < remainder; i++) {
				remainderVotes[i] = malloc(ncandidates * sizeof(int));
				fgets(inputline, 100, fp);
				int j, k, pos = 0;
				for (j = 0, k = 0; j < ncandidates; k++) {
					if (inputline[k] == ' ' || inputline[k] == '\n' || inputline[k] == '\0') {
						sscanf(substring(inputline, pos, k - pos), "%d", &remainderVotes[i][j]);
						j++;
						pos = k + 1;
					}
				}
			}
			for (i = 0; i < remainder; i++) {
				globalCandidateScore[remainderVotes[i][0] - 1]++;
			}
		}
		printf("Round 1:\n");
		for (i = 0; i < ncandidates; i++)
		{
			printf("Candidate %d got %d/%d which is %.2f%%", i + 1, globalCandidateScore[i], nvoters, (globalCandidateScore[i]*1.0/nvoters) * 100);
			if (globalCandidateScore[i] * 1.0 / nvoters > 0.5) {
				printf(" so candidate %d wins in round 1.\n", i + 1);
				extraround = 0;
			}
			else
			{
				putchar('\n');
			}
		}
	}
	if (extraround) {
		int *lastCandidates = malloc(2 * sizeof(int));
		free(localCandidateScore);
		localCandidateScore = malloc(2 * sizeof(int));
		memset(localCandidateScore, 0, 2 * sizeof(int));
		if (rank == 0) {
			lastCandidates[0] = 0;
			lastCandidates[1] = 1;
			for (i = 2; i < ncandidates; i++)
			{
				int j, maxScoreIndex = i;
				for (j = 0; j < 2; j++) {
					if (globalCandidateScore[maxScoreIndex] > globalCandidateScore[lastCandidates[j]]) {
						int temp = maxScoreIndex;
						maxScoreIndex = lastCandidates[j];
						lastCandidates[j] = temp;
					}
				}
			}
			free(globalCandidateScore);
			globalCandidateScore = malloc(2 * sizeof(int));
		}
		MPI_Bcast(lastCandidates, 2, MPI_INT, 0, MPI_COMM_WORLD);
		for (i = 0; i < portionSize; i++) {
			int j, set;
			for (j = 0, set = 0; j < ncandidates; j++)
			{
				int k;
				for (k = 0; k < 2; k++)
				{
					if (votes[i][j] == lastCandidates[k]+1) {
						localCandidateScore[k]++;
						set = 1;
						break;
					}
				}
				if (set) {
					break;
				}
			}
		}
		MPI_Reduce(localCandidateScore, globalCandidateScore, 2, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
		if (rank == 0) {
			if (remainder != 0) {
				for (i = 0; i < remainder; i++) {
					int j, set;
					for (j = 0, set = 0; j < ncandidates; j++)
					{
						int k;
						for (k = 0; k < 2; k++)
						{
							if (remainderVotes[i][j] == lastCandidates[k] + 1) {
								globalCandidateScore[k]++;
								set = 1;
								break;
							}
						}
						if (set) {
							break;
						}
					}
				}
			}
			printf("Round 2:\n");
			for (i = 0; i < 2; i++)
			{
				printf("Candidate %d got %d/%d which is %.2f%%", lastCandidates[i] + 1, globalCandidateScore[i], nvoters, (globalCandidateScore[i] * 1.0 / nvoters) * 100);
				if (globalCandidateScore[i] * 1.0 / nvoters > 0.5) {
					printf(" so candidate %d wins in round 2.\n", lastCandidates[i] + 1);
				}
				else
				{
					putchar('\n');
				}
			}
		}
	}
	fclose(fp);
	MPI_Finalize();
	return 0;
}