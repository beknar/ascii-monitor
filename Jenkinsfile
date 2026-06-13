// Build ascii-monitor on every node in the Jenkins fleet.
//
// The fleet's controller is gemini; the build nodes are abba, calibri, gabriel,
// zariel, taroo (Linux), lulu, jindi, jindi2 (FreeBSD) and phoenix (Solaris).
// Each node's name is also one of its labels, so `agent { label "${NODE}" }`
// pins one matrix cell per host. Cells run in parallel and are independent
// (failFast is off by default), so a failure on one node doesn't stop the rest.
//
// The build itself is just `gmake` (GNU make exists on every node; on Linux it's
// a symlink to make). The Makefile handles the per-OS specifics (e.g. -lkstat and
// the ncurses include path on Solaris), so every node runs the same command.
// ascii-monitor builds and runs on Linux, FreeBSD and Solaris, but this job is
// build-only: it's an interactive ncurses TUI with no batch/test mode, so there's
// nothing to execute in CI.

pipeline {
    agent none

    options {
        timestamps()
        buildDiscarder(logRotator(numToKeepStr: '20'))
    }

    stages {
        stage('Build on the fleet') {
            matrix {
                axes {
                    axis {
                        name 'NODE'
                        values 'abba', 'calibri', 'gabriel', 'zariel', 'taroo',
                               'lulu', 'jindi', 'jindi2', 'phoenix'
                    }
                }

                agent { label "${NODE}" }

                stages {
                    stage('Compile') {
                        steps {
                            echo "Building ascii-monitor on ${NODE} (node=${env.NODE_NAME}, labels=${env.NODE_LABELS})"
                            sh '''
                                set -eu
                                # The Makefile applies the per-OS flags itself (Solaris adds the
                                # ncurses include path and -lkstat), so the command is identical
                                # on every node.
                                gmake clean || true
                                gmake

                                ls -l ascii-monitor
                                # Give each node's binary a distinct name so the archived
                                # artifacts from all cells don't collide.
                                cp ascii-monitor "ascii-monitor-${NODE_NAME}"
                            '''
                        }
                    }
                }

                post {
                    success {
                        archiveArtifacts artifacts: "ascii-monitor-${env.NODE_NAME}", fingerprint: true
                    }
                }
            }
        }
    }
}
