plugins {
    java
}

repositories {
    mavenCentral()
}

dependencies {
    // scala-library version must match the _2.13 suffix used by bitcoin-s
    implementation("org.scala-lang:scala-library:2.13.12")
    implementation("org.bitcoin-s:bitcoin-s-core_2.13:1.9.9")
}

java {
    toolchain {
        languageVersion = JavaLanguageVersion.of(21)
    }
}

tasks.register<Copy>("copyDeps") {
    from(configurations.runtimeClasspath)
    into(layout.projectDirectory.dir("../lib"))
}
