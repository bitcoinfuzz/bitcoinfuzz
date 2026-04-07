plugins {
    // Apply the Scala plugin
    scala

    // Apply the application plugin to add support for building a CLI application
    application
}

repositories {
    mavenCentral()
}

dependencies {
    // bitcoin-s core library
    implementation("org.bitcoin-s:bitcoin-s-core_2.13:${libs.versions.bitcoins.get()}")

    // Scala standard library
    implementation("org.scala-lang:scala-library:${libs.versions.scala.get()}")
}

// Apply a specific Java toolchain to ease working on different environments.
java { toolchain { languageVersion = JavaLanguageVersion.of(21) } }
