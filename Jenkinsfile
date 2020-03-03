
pipeline {
	agent any

	options {
		timeout(time: 15, unit: 'MINUTES')
	}
	stages {
		stage('Build') {
			steps {
				sh 'echo "Executing build steps..."'
				cmakeBuild cleanBuild: true, buildDir: 'build', installation: 'InSearchPath', steps: [[args: 'all', envVars: 'DESTDIR=${WORKSPACE}/artifacts', withCmake: true]]
			}
		}
		stage('Run tests') {
			parallel {
				stage('Integration tests') {
					steps {
						sh 'echo "Running Maestro integration tests..."'
						sh './allMaestroIntegrationTests.sh'
					}
				}
				stage('Format check') {
					steps {
						sh 'echo "Running code formatting check..."'
						sh './checkCodeFormat.sh'
					}
				}
	    		}
		}
	}
}


