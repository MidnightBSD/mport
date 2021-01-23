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
    }
}
