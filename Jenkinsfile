// Build ascii-monitor on every node in the Jenkins fleet.
//
// The fleet's controller is gemini; the build nodes are abba, calibri, gabriel,
// zariel, taroo (Linux), lulu, jindi, jindi2 (FreeBSD) and phoenix (Solaris).
// Each node's name is also one of its labels, so `agent { label "${NODE}" }`
// pins one matrix cell per host. Cells run in parallel and are independent
// (failFast is off by default), so a failure on one node doesn't stop the rest.
//
// The build itself is just `gmake` (GNU make exists on every node; on Linux it's
// a symlink to make). ascii-monitor is a Linux-only program at runtime (it reads
// /proc), but it *compiles* everywhere, so this job is build-only — it never runs
// the binary (it's an interactive ncurses TUI with no batch/test mode).

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
                                # ascii-monitor links -lncurses. On Solaris the ncurses headers
                                # live under /usr/include/ncurses (off the default search path),
                                # so point the compiler at them via the env var rather than
                                # editing the repo Makefile. No-op on Linux/FreeBSD.
                                if [ "$(uname -s)" = "SunOS" ]; then
                                    export CPLUS_INCLUDE_PATH=/usr/include/ncurses
                                fi

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
