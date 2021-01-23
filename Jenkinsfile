pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                sh '/usr/local/sonar/build-wrapper-linux-x86/build-wrapper-linux-x86-64  --out-dir . make clean all' 
            }
        }
        stage('Sonarqube') {
            steps {
                withSonarQubeEnv('sonarcloud') {
                	sh "${tool("sonarqube")}/bin/sonar-scanner -Dsonar.organization=laffer1-github"
                }
                timeout(time: 10, unit: 'MINUTES') {
                    waitForQualityGate abortPipeline: true
                }
            }
       }
    }
}
