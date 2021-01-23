pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                sh 'make' 
            }
        }
        stage('Cleanup') {
            steps {
                sh 'make clean' 
            }
        }
        stage('Sonarqube') {
            steps {
                withSonarQubeEnv('sonarcloud') {
                	sh 'mvn sonar:sonar -Dsonar.organization=laffer1-github'
                }
                timeout(time: 10, unit: 'MINUTES') {
                    waitForQualityGate abortPipeline: true
                }
            }
       }
    }
}
