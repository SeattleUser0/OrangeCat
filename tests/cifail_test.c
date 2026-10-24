/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Throwaway test for exp/sonnet-cifail only: fails on purpose, to show that the CI run goes red (job tests-gate) while the
 * packages artifact is still there. Never merged. */
#include <assert.h>
#include <stdio.h>
int main(void)
{
    printf("cifail: the deliberate assert follows\n");
    assert(1 == 2 && "deliberate failure for the CI gate check");
    return 0;
}
