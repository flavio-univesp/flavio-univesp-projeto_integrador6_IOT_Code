
<p align="center"> <i>Desenvolvido com dedicação pelo grupo <strong>CondoAcessos</strong> — Projeto Integrador em Computação VI (UNIVESP, 2026)</i> </p> </div> ```





<p align="center">
  <img src="https://user-images.githubusercontent.com/50468352/141820811-412e9364-7f5c-4889-826a-fcba23b92e23.png" width="350" alt="Logo do Projeto" />
</p>

<h3 align="center">📌 Projeto Integrador em Computação VI - 2026</h3>

<p align="center"><strong>Polo:</strong> DRP04 - Trocar pelo nome dos polos</p>
<p align="center"><strong>Orientadora do PI:</strong> Aline Santana</p>

---

## 👥 Integrantes do grupo

| Nome                                | RA         |
|-------------------------------------|------------|
| Daniel Anunciato                    |  2222677   |
| Eder Clauber dos Santos dos Anjos   |  1806662   |
| Felipe Rafael Henriques             |  2214261   |
| Flavio Jorge de Medeiros            | 23205233   |
| Francisco Ribeiro da Silva Junior   |  2108392   |
| Kelven Joseph Machado Santos        |  2100626   |
| Matheus Eduardo Peixoto de Carvalho |  2205301   |
| Nicolly de Sousa Lima               |  2205907   |



---

## 💡 Projeto: *CondoAcessos* - Nome ainda necessia de definição

> **Sistema de controle de acessos com histórico para condomínios.**

---

<details>
<summary>⚠️ <strong>Sobre o desafio encontrado</strong></summary>
<br/>

🔍 Tivemos o desafio de solucionar as dificuldades de disponibilidade (sistema que nao esteja dependente de servidor físico) e ter um gerenciamento das informações de controle de acessos de moradores de forma centralizada, acessível e segura para condomínios.

Nesse contexto, propusemos o desenvolvimento de uma plataforma web em nuvem, acessível por múltiplos dispositivos e com integração de banco de dados e a criação de um dispositivo IOT para verificação on-line de acesso, permitindo assim um gerenciamento de acesso seguro, com controles de registro de acessos por moradores.

</details>

---

<details>
<summary>🎯 <strong>Solução implementada</strong></summary>
<br/>

✅ Desenvolver um software com framework web em nuvem que utilize banco de dados, inclua script web (Javascript), nuvem (Microsoft Azure), uso de API, acessibilidade, controle de versão e testes. 
Permitindo o acesso independente do lugar, pelos moradores quanto as funcionalidades de cadastro e consulta de prestadores; controle de documentação e da base de dados e de seus respectivos históricos e acesso por múltiplos dispositivos.

✅ Desenvolver 2 IOT (um para identificação do TAG ID para cadastro com o Condomino e um para fazer o controle de acesso e a comunicação com a nuvem)

✅ A proposta contribui para a modernização da gestão condominial, reduzindo falhas de controler administrativos, adoção evitar o uso de produtos com alto custo e aumentando o controle por parte da administração dos acessos ao interior dos condomínios

<p align="center">
<! --  <img src="projeto_integrador_1/vitrine.jpg" width="600" alt="Imagem da Vitrine Web">
</p>

</details>

---

<details>
<summary>⚙️ <strong>Estrutura do projeto</strong></summary>
<br/>

O sistema foi estruturado no modelo **cliente-servidor em nuvem**, com interface web conectada a uma API REST e banco de dados hospedado na **plataforma Azure**.

Frontend (HTML, CSS, JS) → Backend (Node.js + Express) → Banco de Dados (MySQL no Azure) → Hospedagem e monitoramento (Azure App Service + Azure Monitor).

Arduino como plataforma IOT de identificação da TAG ID para associação a cada condômino

ESP32X como plataforma de leitura de TAG e comunicação com o APP em nuvem para liberação de acesso

</details>

---

<details>
<summary>🛠️ <strong>Como rodar o projeto localmente</strong></summary>
<br/>

✅ **Clonar o projeto para a máquina local:**  
 <code>git clone flavio-univesp/projeto_integrador6</code>

</br>

✅ **Acesse o diretório do projeto:**  
Navegue para o diretório do projeto clonado usando o comando:  
 <code>cd projeto_integrador6</code>

</br>

📄 O sistema está disponível no navegador em (disponível apenas no 2 semestre de 2026):

👉 **A Definir**

</details>

---

## 🧰 Tecnologias e ferramentas utilizadas

<p>
  <img src="https://img.shields.io/badge/JavaScript-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black" alt="JavaScript Badge"/>
  <img src ="https://img.shields.io/badge/microsoft%20azure-0089D6?style=for-the-badge&logo=microsoft-azure&logoColor=white"/>
  <img src="https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white" alt="HTML5 Badge"/>
  <img src="https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css3&logoColor=white" alt="CSS3 Badge"/>
  <img src="https://img.shields.io/badge/GitHub-181717?style=for-the-badge&logo=github&logoColor=white" alt="GitHub Badge"/>
</p>


