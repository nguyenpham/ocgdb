# Open Chess Game Database Standard (OCGDB)

## Overview

Almost all popular/published chess game databases are in binary formats. Those formats have some advantages such as fast and compact. However, they also have some drawbacks:
 
- Hard to understand: each format requires a long list of descriptions and explanations why this but not that
- Hard to support by other apps: other programs may get many problems to support since they can use different programming languages, data structures…
- Hard to support by web apps: processing large binary files is not strong points of scripting languages/web apps
- License compatibility: code for those binary formats are mostly from GPL to more restricted, thus some programs will get hard to adapt. 

Typically a format is sticked to a specific chess software and the exchange between formats is very limited.

The advantages of being binary formats were very important in the past when computers were weak and Internet speed was slow. However, they become less important nowadays since computers become much stronger and Internet is much faster. That has been making other formats become more attractive.

On the other hand, we have the PGN format for chess games, which could be used for chess databases too. It has some advantages:
- It is a kind of open standard already. Almost all chess GUIs and tools understand and can work with
- Readable for both human and software (because of being text format). We don’t need any special tool to read and edit, just any text editor
- So simple to understand and support


However, there are many drawbacks for using PGN as chess databases. 
- It doesn’t have structures at all, there is no relationship between games, players, events. They are independent to each other. In other words, they are just a collection of games, not databases.
- The data may be heavily redundant and inconsistent: names of events, players may be repeated multi times. Same events, players may be different in some games
- Processing speed is so low (compared with binary databases), especially when the number of games large. Even some simple queries such as count numbers of games, players,… may take long and unacceptable periods in modern computers
- Text size is typically large


Thus we think we need a new format which could be used to exchange databases between different apps and/or for web apps. Requirements:
- Easier to understand data structures. It is the best if it could describe itself
- Easy to support by different languages, apps
- Fast enough for some apps to use directly
- Easy to support and fast enough to use by web apps
- Almost free for all (MIT/or less restriction license)

## SQL/SQLite
For starting, we have picked up SQL/SQLite as the main tools/direction to develop the new database. Using SQL for chess game database is not new, someone have tried already (and gave up). We have known it has some drawbacks such as:
- Slow
- Large on size

However it has some advantages:
- Data in text forms, readable for both human and machines
- Supported by a lot of tools
- Easy to understand structures. They all are almost described themselves 
- Easy to write tools, converters
- Users can query directly


This project is just a work in progress. We are totally open mind. Nothing is fixed. Everything could be changed, from data structures to main programming language, tools, directions... We listen to you all and welcome any help, distribution. You may discuss here or in GibHub (discuss section). Feel free to push requests.


## Working on
- Understand, measure speeds, sizes, drawbacks, problems
- Develop all basic database functions
- Create a huge database of over 5 million games
- Benchmark (to speed up later)
- Improve speed/size

In other words, we want to try and push the boundary as far as we could.


## Data and code

### Sample
There is a sample database in the folder samples, named games.ocgdc.db3 with over 50000 games. It could be open by any SQLite browser. You can download, open it and make some query to understand its structures, speed, advantages and disadvantages.

### Code

#### SQL commands
The file SqlCmd.md contain some SQL commands to create databases and tables, examples to insert and query tables.

#### Cpp code
All same, tools will be developed and released in C++ 17.

## License
MIT: Almost totally free (requires just fair use). All documents, codes, data samples in this project are ready and free to integrate into other apps without worrying about license compatibility.


