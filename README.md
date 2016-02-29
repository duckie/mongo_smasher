MongoDB Smasher
==================

This project is a tool to fill up MongoDB databases with huge sets of randomized data to test your architecture and your data model performances.

# How to build

The *only* local dependency is `boost`, be sure it is installed along with a modern C++ compiler and `cmake` then:

	git clone https://github.com/duckie/mongo_smasher.git
	cd mongo_smasher
	git submodule update --init --recursive
	mkdir build
	cd build
	cmake -DCMAKE_BUILD_TYPE:STRING=Release ../
	make -j${number_of_machine_CPUs}

The generated executable is found as `mongo_smasher/build/src/mongo_smasher`. Optional:

	make install

**Tip**: Do not use more CPUs than your machine has with `make`or the compilation of the mongo C++11 driver could fail.

**Tip**: If you use Windows, make yourself a favor and install [cash](https://github.com/dthree/cash).

# How to use

`mongo_smasher` must be fed a json file containing your collections and schemas. Check the `examples` directory. Here is one of them:

    {
      "values":{
        "id":{"type":"incremental_id"},
        "restaurant_seed":{"type":"string","min_size":10,"max_size":150},
        "score":{"type":"int","min":-1,"max":131},
        "grade":{"type":"pick", "values":["A","B","C","D","E","F"]},
        "grade_date":{"type":"date","min":"2012-01-01 00:00:00","max":"2016-02-29 00:00:00"},
        "cuisine":{"type":"pick","file":"ny/cuisines.txt"},
        "nyzip":{"type":"pick","file":"ny/zipcodes.txt"},
        "nyst":{"type":"pick","file":"ny/street-names.txt"},
        "nybuilding":{"type":"pick","file":"ny/buildings.txt"},
        "borough":{"type":"pick","values":["Missing","Queens","Bronx","Brooklyn","Manhattan" ,"Staten Island"]},
        "geo_x":{"type":"double","min":-28.0168595,"max":52.5388779},
        "geo_y":{"type":"double","min":-157.8887924,"max":153.1628795},
        "price":{"type":"double","min":1.0,"max":500.0},
        "firstname":{"type":"pick","file":"dictionaries/first-names.txt"},
        "lastname":{"type":"pick","file":"dictionaries/names.txt"}
      },
      "collections":{
        "restaurants":{
          "schema":{
            "address":{
              "building":"$nybuilding",
              "street":"$nyst",
              "coord":["$geo_x","$geo_y"],
              "zipcode":"$nyzip"
            },
            "borough":"$borough",
            "cuisine?0.7":"$cuisine",
            "grades[0:4]":{
              "date":"$grade_date",
              "grade":"$grade",
              "score":"$score"
            },
            "name":"At $firstname $lastname's $cuisine $estaurant_seed",
            "restaurant_id":"$id"
          },
          "bulk_size":1000
        }
      }
    }

Use `--help` switch to get some details about command line options.

# Contribute

The tool is in very active development. Any contribution is welcome.
