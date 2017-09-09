package main

import (
	"errors"
	"fmt"

	"github.com/opencontainers/runc/libcontainer"

	"github.com/urfave/cli"
	"os"
	"log"

	io "io/ioutil"
	"encoding/json"
)

func extend(id string) error {

	// define the log file
	var filename = "/var/log/docker-run.log"
	logFile, err  := os.Create(filename)
	defer logFile.Close()
	if err != nil {
		log.Fatalln("open file error !")
		return err
	}
	debugLog := log.New(logFile, "[Debug]", log.Llongfile)

	// get the config of this container, and parse
	debugLog.Print(">>> handle ... ")
	debugLog.Println(id)

	// load config file which includes the information of image
	var dict map[string]interface{}
	configFileName := "/var/lib/docker/containers/" + id + "/config.v2.json"
	data, err := io.ReadFile(configFileName)
	if err != nil {
		debugLog.Fatalln("cannot find image")
		return err
	}

	datajson := []byte(data)
	err = json.Unmarshal(datajson, &dict)
	if err != nil {
		debugLog.Fatalln("failed to load config")

	}

	if imageDigest, ok := dict["Image"].(string); ok {
		debugLog.Println(">> The image digest is: " + imageDigest)
	}

	return nil
}

var startCommand = cli.Command{
	Name:  "start",
	Usage: "executes the user defined process in a created container",
	ArgsUsage: `<container-id>

Where "<container-id>" is your name for the instance of the container that you
are starting. The name you provide for the container instance must be unique on
your host.`,
	Description: `The start command executes the user defined process in a created container.`,
	Action: func(context *cli.Context) error {
		if err := checkArgs(context, 1, exactArgs); err != nil {
			return err
		}
		container, err := getContainer(context)
		if err != nil {
			return err
		}

		status, err := container.Status()
		if err != nil {
			return err
		}
		switch status {
		case libcontainer.Created:
			err = container.Exec()
			return err
		case libcontainer.Stopped:
			return errors.New("cannot start a container that has stopped")
		case libcontainer.Running:
			return errors.New("cannot start an already running container")
		default:
			return fmt.Errorf("cannot start a container in the %s state\n", status)
		}
	},
	After:func(context *cli.Context) error {
		container, err := getContainer(context)
		if err != nil {
			return err
		}

		//id := container.ID()
		err = extend(container.ID())
		return err
	},
}
